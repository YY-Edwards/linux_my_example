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

	//是特意用于底层的强制转型，导致实现依赖（就是说，不可移植）的结果，例如，将一个指针转型为一个整数
	uint8_t* start	= reinterpret_cast<uint8_t*>(buf->beginWrite());
	uint8_t* end = message.SerializeWithCachedSizesToArray(start);//从start处起，写入数据，并可以获取message的末尾指针
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


	buf->appendInt32(checkSum);//默认处理成网络字节序

	assert(buf->readableBytes() == sizeof nameLen + nameLen + byte_size + sizeof checkSum);
	int32_t len = sockets::hostToNetwork32(static_cast<int32_t>(buf->readableBytes()));//主机转换成大端字节序
	buf->prepend(&len, sizeof len);//前置添加长度

}
 //匿名命名
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
	 ::memcpy(&be32, buf, sizeof be32);//字符串转整形
	 return sockets::networkToHost32(be32);//网络字节序转换成主机字节序 
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
			 message.reset(createMessage(typeName));//message先重置，然后接管createMessage返回的指针。
			 if (message)
			 {
				 // parse from buffer
				 const char* data = buf + kHeaderLen + nameLen;
				 int32_t dataLen = len - nameLen - 2 * kHeaderLen;
				 //将data设置给message对象:反序列化
				 auto ret = message->ParseFromArray(data, dataLen);
				 if (ret)//解析成功
				 {
					 //回传错误类型
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
		 //取出协议头中的长度
		 const int32_t len = buf->peekInt32();//为了跨语言移植，java中没有unsigned
		 if (len > kMaxMessageLen || len < kMinMessageLen)//超过设定的最高门限
		 {
			 errorCallback_(conn, buf, receviceTime, kinvalidLength);
			 break;//默认操作直接disconnect peer,考虑是否需要手动偏移buf
		 }
		 else if (buf->readableBytes() >= implicit_cast<size_t>(len + kHeaderLen))//完整或粘包都可以通过
		 {
		//implicit_cast只在特殊情况下才有必要，即当一个表达式的类型必须被精确控制时，比如说，为了避免重载（overload）。
		//implicit_cast相较于其它转型的好处是，读代码的人可以立即就明白，这只是一个简单的隐式转换，不是一个潜在的危险的转型（不完全正确，见下文）。
			 ErrorCode errorCode = kNoError;
			 //将Buffer转变成Message类型
			 MessagePtr message = parse(buf->peek() + kHeaderLen, len, &errorCode);
			 if (errorCode == kNoError && message)//parse okay
			 {
				 //形参为引用，实参传递过程不拷贝数据。
				 messageCallback_(conn, message, receviceTime);//回调上层注册的函数
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
			 break;//等待一个完整包到达
		 }
	 }
	
 }
