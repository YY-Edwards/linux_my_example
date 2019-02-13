/**
* Protobuf codec implementation,Codec.cpp
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180212
*/


#include "Codec.h"

#include <muduo/base/Logging.h>
#include <muduo/net/Endian.h>
#include <muduo/net/protorpc/google-inl.h>

#include <google/protobuf/descriptor.h>

#include <zlib.h>  // adler32

using namespace muduo;
using namespace muduo::net;

google::protobuf::Message* ProtobufCodec::createMessage(const std::string& typeName)
{

	google::protobuf::Message* message = NULL;
	const google::protobuf::Descriptor* descriptor =
		google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(typeName);
	if (descriptor)
	{
		const google::protobuf::Message* prototype =
			google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
		if (prototype)
		{
			message = prototype->New();
		}
	}
	return message;

}


void ProtobufCodec::fillEmptyBuffer(muduo::net::Buffer* buf, const google::protobuf::Message& message)
{

	assert(buf->readableBytes() == 0);
	const std::string& typeName = message.GetTypeName();
	int32_t nameLen = static_cast<int32_t>(typeName.size() + 1);//+"\0";
	buf->appendInt32(nameLen);
	buf->append(typeName.c_str(), nameLen);

	// code copied from MessageLite::SerializeToArray() and MessageLite::SerializePartialToArray().
	GOOGLE_DCHECK(message.IsInitialized()) << InitializationErrorMessage("serialize", message);

	int byte_size = message.ByteSize();
	buf->ensureWritableBytes(byte_size);

	//���������ڵײ��ǿ��ת�ͣ�����ʵ������������˵��������ֲ���Ľ�������磬��һ��ָ��ת��Ϊһ������
	uint8_t* start	= reinterpret_cast<uint8_t*>(buf->beginWrite());
	uint8_t* end = message.SerializeWithCachedSizesToArray(start);//��start����д�����ݣ������Ի�ȡmessage��ĩβָ��
	if (end - start != byte_size)//test size
	{
		ByteSizeConsistencyError(byte_size, message.ByteSize(), static_cast<int>(end - start));
	}
	buf->hasWritten(byte_size);
	
	//checksum
	int32_t checkSum = static_cast<int32_t>(
		::adler32(1,
		reinterpret_cast<const Bytef*>(buf->peek()),//Bytef ?= char *
		static_cast<int>(buf->readableBytes())));


	buf->appendInt32(checkSum);//Ĭ�ϴ���������ֽ���

	assert(buf->readableBytes() == sizeof nameLen + nameLen + byte_size + sizeof checkSum);
	int32_t len = sockets::hostToNetwork32(static_cast<int32_t>(buf->readableBytes()));//����ת���ɴ���ֽ���
	buf->prepend(&len, sizeof len);//ǰ����ӳ���

}
 //��������
 namespace
 {
	 const string kNoErrorStr = "NoError";
	 const string kInvalidLengthStr = "InvalidLength";
	 const string kCheckSumErrorStr = "CheckSumError";
	 const string kInvalidNameLenStr = "InvalidNameLen";
	 const string kUnknownMessageTypeStr = "UnknownMessageType";
	 const string kParseErrorStr = "ParseError";
	 const string kUnknownErrorStr = "UnknownError";
 }

 const muduo::string& ProtobufCodec::errorCodeToString(ErrorCode errorCode)
 {
 
	 switch (errorCode)
	 {
	 case kNoError:
		 return kNoErrorStr;
	 case kinvalidLength:
		 return kInvalidLengthStr;
	 case kCheckSumError:
		 return kCheckSumErrorStr;
	 case kInvalidNameLen:
		 return kInvalidNameLenStr;
	 case kUnkownMessageType:
		 return kUnknownMessageTypeStr;
	 case kParseError:
		 return kParseErrorStr;
	 default:
		 return kUnknownErrorStr;
	 }
 
 }


 void ProtobufCodec::defaultErrorCallback(const muduo::net::TcpConnectionPtr& conn,
	 muduo::net::Buffer* buf,
	 muduo::Timestamp receiveTime,
	 ErrorCode errorCode)
 {
	 LOG_ERROR << "ProtobufCodec::defaultErrorCallback - " << errorCodeToString(errorCode);
	 if (conn && conn->connected())
	 {
		 conn->shutdown();
	 }
 }

 int32_t asInt32(const char* buf)
 {
	 int32_t be32 = 0;
	 ::memcpy(&be32, buf, sizeof be32);//�ַ���ת����
	 return sockets::networkToHost32(be32);//�����ֽ���ת���������ֽ��� 
 }




 MessagePtr ProtobufCodec::parse(const char* buf, int len, ErrorCode* errorCode)
 {
	 MessagePtr message;

	 //check sum
	 int32_t expectedCheckSum = asInt32(buf + len - kHeaderLen);
	 int32_t checkSum = static_cast<int32_t>(
		 ::adler32(1,
		 reinterpret_cast<const Bytef*>(buf),//Bytef ?= char *
		 static_cast<int>(len - kHeaderLen)));
	 if (expectedCheckSum == checkSum)
	 {
		 //get message type name 
		 int32_t nameLen = asInt32(buf);
		 if (nameLen >= 2 && nameLen <= len - 2 * kHeaderLen)
		 {
			 std::string typeName(buf + kHeaderLen, buf + kHeaderLen + nameLen - 1);
			 LOG_DEBUG << "receive typeName :[" << typeName << "]";
			 // create message object
			 message.reset(createMessage(typeName));//message�����ã�Ȼ��ӹ�createMessage���ص�ָ�롣
			 if (message)
			 {
				 // parse from buffer
				 const char* data = buf + kHeaderLen + nameLen;
				 int32_t dataLen = len - nameLen - 2 * kHeaderLen;
				 //��data���ø�message����:�����л�
				 auto ret = message->ParseFromArray(data, dataLen);
				 if (ret)//�����ɹ�
				 {
					 //�ش���������
					 *errorCode = kNoError;
				 }
				 else
				 {
					 *errorCode = kParseError;
				 }
			 }
			 else
			 {
				 *errorCode = kUnkownMessageType;
			 }
		 }
		 else
		 {
			 *errorCode = kInvalidNameLen;
		 }
	 }
	 else
	 {
		 *errorCode = kCheckSumError;
	 }

	 return message;
 }

 void ProtobufCodec::onMessage(const muduo::net::TcpConnectionPtr& conn,
								 muduo::net::Buffer* buf,
								 muduo::Timestamp  receviceTime)
 {
	 while (buf->readableBytes() >= kMinMessageLen + kHeaderLen)
	 {
		 //ȡ��Э��ͷ�еĳ���
		 const int32_t len = buf->peekInt32();//Ϊ�˿�������ֲ��java��û��unsigned
		 if (len > kMaxMessageLen || len < kMinMessageLen)//�����趨���������
		 {
			 errorCallback_(conn, buf, receviceTime, kinvalidLength);
			 break;//Ĭ�ϲ���ֱ��disconnect peer,�����Ƿ���Ҫ�ֶ�ƫ��buf
		 }
		 else if (buf->readableBytes() >= implicit_cast<size_t>(len + kHeaderLen))//������ճ��������ͨ��
		 {
		//implicit_castֻ����������²��б�Ҫ������һ�����ʽ�����ͱ��뱻��ȷ����ʱ������˵��Ϊ�˱������أ�overload����
		//implicit_cast���������ת�͵ĺô��ǣ���������˿������������ף���ֻ��һ���򵥵���ʽת��������һ��Ǳ�ڵ�Σ�յ�ת�ͣ�����ȫ��ȷ�������ģ���
			 ErrorCode errorCode = kNoError;
			 //��Bufferת���Message����
			 MessagePtr message = parse(buf->peek() + kHeaderLen, len, &errorCode);
			 if (errorCode == kNoError && message)//parse okay
			 {
				 //�β�Ϊ���ã�ʵ�δ��ݹ��̲��������ݡ�
				 messageCallback_(conn, message, receviceTime);//�ص��ϲ�ע��ĺ���
				 buf->retrieve(kHeaderLen + len);

			 }
			 else
			 {
				 errorCallback_(conn, buf, receviceTime, errorCode);
				 break;
			 }

		 }
		 else
		 {
			 break;//�ȴ�һ������������
		 }
	 }
	
 }
