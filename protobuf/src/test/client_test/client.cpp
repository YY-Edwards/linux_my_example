/**
* Protobuf client implementation,client.h
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180218
*/



#include "Codec.h"
#include "Dispatcher.h"
#include "query_proto2.pb.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>

#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo :: net;

google::protobuf::Message* messageToSend = NULL;

class QueryClient:noncopyable
{
public:


	typedef std::shared_ptr<edwards::Query> QueryPtr;
	typedef std::shared_ptr<edwards::Answer> AnswerPtr;
	typedef std::shared_ptr<edwards::Empty> EmptyPtr;

	explicit QueryClient(EventLoop *loop,
						const InetAddress& serverAddr,
						const std::string& name)
						:loop_(loop)
						, client_(loop, serverAddr, name)
						, dispatcher_(std::bind(&QueryClient::onUnknowMessage, this, _1, _2, _3))//注册一个无法识别的默认的回调
						//还可以注册一个解析出错的用户回调
						, codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3), NULL)
	{

		//注册收到确切protobuf类型的消息回调
		dispatcher_.registerMessageCallback<edwards::Answer>(
			std::bind(&QueryClient::onAnswer, this, _1, _2, _3));
		dispatcher_.registerMessageCallback<edwards::Empty>(
			std::bind(&QueryClient::onEmpty, this, _1, _2, _3));

		client_.setConnectionCallback(
			std::bind(&QueryClient::onConnection, this, _1));
		client_.setMessageCallback(
			std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));

	}
	~QueryClient() =default;

	void connect()
	{
		client_.connect();
		//client_.enableRetry();//开启断开重连功能
	}


	void disconnect()
	{
		client_.disconnect();
	}

private:


	void onAnswer(const muduo::net::TcpConnectionPtr& conn,
				  const AnswerPtr& message,
				  muduo::Timestamp t)
	{
		LOG_DEBUG << "onAnswer: \n" << message->GetTypeName()
			<< message->DebugString();

		 message->PrintDebugString();

	}

	void onEmpty(const muduo::net::TcpConnectionPtr& conn,
				 const EmptyPtr& message,
				 muduo::Timestamp t)
	{

		LOG_DEBUG << "onEmpty: \n" << message->GetTypeName()
			<< message->DebugString();

		 message->PrintDebugString();
	}

	void onUnknowMessage(const TcpConnectionPtr& conn,
						const MessagePtr& message,
						muduo::Timestamp)
	{
		LOG_DEBUG << "onUnknowMessage: \n" << message->GetTypeName();
	}

	void onConnection(const TcpConnectionPtr& conn)
	{
		LOG_INFO << conn->localAddress().toIpPort() << " -> "
			<< conn->peerAddress().toIpPort() << " is "
			<< (conn->connected() ? "UP" : "DOWN");

		if (conn->connected())
		{
			if (messageToSend)
				codec_.send(conn, *messageToSend);
		}
		else
		{
			loop_->quit();
		}
	}



	EventLoop*			loop_;
	TcpClient			client_;
	ProtobufDispatcher	dispatcher_;
	ProtobufCodec		codec_;

};





int main(int argc, char* argv[])
{

	Logger::setLogLevel(Logger::TRACE);
	LOG_INFO << "pid = " << getpid();

	if (argc == 2)
	{
		string hostport = argv[1];
		auto colon = hostport.find(':');
		if (colon != string::npos)
		{
			string hostip = hostport.substr(0, colon);
			const uint16_t port = static_cast<uint16_t>(atoi(hostport.c_str() + colon + 1));

			edwards::Query query;
			query.set_id(9);
			query.set_questioner("Edwards");
			query.add_question("Running?");

			messageToSend = &query;

			EventLoop loop;
			QueryClient client(&loop, InetAddress(hostip, port), "QueryClient");
			client.connect();
			loop.loop();

		}
		else
		{
			printf("Usage: %s host_ip:port  \n", argv[0]);
		}
	}
	else
	{
		printf("Usage: %s host_ip port [q|e]\n", argv[0]);
	}


	printf("\r\n=>>exit main.cpp \r\n");

}


