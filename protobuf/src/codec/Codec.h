/**
* Protobuf codec header,Codec.h
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180212
*/

#ifndef __PROTOBUF_CODEC_H
#define __PROTOBUF_CODEC_H

#include <muduo/net/Buffer.h>
#include <muduo/net/TcpConnection.h>

#include <google/protobuf/message.h>

//simple protobuf struct

// struct ProtobufTransportFormat __attribute__ ((__packed__))
// {
//   int32_t  len;>=10
//   int32_t  nameLen;>=2
//   char     typeName[nameLen];
//   char     protobufData[len-nameLen-8];
//   int32_t  checkSum; // adler32 of nameLen, typeName and protobufData
// }

typedef std::shared_ptr<google::protobuf::Message> MessagePtr;



class ProtobufCodec : muduo::noncopyable
{
public:

	enum ErrorCode
	{
		kNoError = 0,
		kinvalidLength,
		kCheckSumError,
        kInvalidNameLen,
		kUnkownMessageType,
		kParseError,
	};

	typedef std::function<void(const muduo::net::TcpConnectionPtr&, 
								const MessagePtr&,
								muduo::Timestamp)> ProtobufMessageCallback;

	typedef std::function<void(const muduo::net::TcpConnectionPtr&,
								muduo::net::Buffer*,
								muduo::Timestamp,
								ErrorCode)> ErrorCallback;





	explicit ProtobufCodec(const ProtobufMessageCallback& messageCb)
		:messageCallback_(messageCb),
		errorCallback_(defaultErrorCallback)//set default error callback
	{
	}

	ProtobufCodec(const ProtobufMessageCallback& messageCb, const ErrorCallback& errorCb)
		:messageCallback_(messageCb),
		errorCallback_(errorCb)
	{
	}

	~ProtobufCodec () = default;


	void onMessage(const muduo::net::TcpConnectionPtr& conn,
					muduo::net::Buffer* buf,
					muduo::Timestamp  receviceTime);

	void send(const muduo::net::TcpConnectionPtr& conn,
		const google::protobuf::Message& message)
	{
		muduo::net::Buffer buf;
		fillEmptyBuffer(&buf, message);//package google::message to Buffer
		conn->send(&buf);
	}

	static const muduo::string& errorCodeToString(ErrorCode errorCode);
	static void fillEmptyBuffer(muduo::net::Buffer* buf, const google::protobuf::Message& message);
	static google::protobuf::Message* createMessage(const std::string& typeName);
	static MessagePtr parse(const char* buf, int len, ErrorCode* errorCode);

private:

	//default error callback
	static  void defaultErrorCallback(const muduo::net::TcpConnectionPtr&,
											muduo::net::Buffer*,
											muduo::Timestamp,
											ErrorCode);


	ProtobufMessageCallback messageCallback_;
	ErrorCallback			errorCallback_;

	const static int kHeaderLen = sizeof(int32_t);
	const static int kMinMessageLen = 2 * kHeaderLen + 2; // nameLen + typeName + checkSum:>=10
	const static int kMaxMessageLen = 64 * 1024 * 1024; // same as codec_stream.h kDefaultTotalBytesLimit

};



#endif  // __PROTOBUF_CODEC_H