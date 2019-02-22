/**
* Protobuf server implementation,server.h
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180220
*/



#include "Codec.h"
#include "Dispatcher.h"
#include "query_proto2.pb.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

google::protobuf::Message* messageToSend = NULL;

class QueryServer :noncopyable
{
public:


	typedef std::shared_ptr<edwards::Query> QueryPtr;
	typedef std::shared_ptr<edwards::Answer> AnswerPtr;
	typedef std::shared_ptr<edwards::Empty> EmptyPtr;

	explicit QueryServer(EventLoop *loop,
		const InetAddress& listenAddr,
		const std::string& name)
		:loop_(loop)
		, server_(loop, listenAddr, name)
		, dispatcher_(std::bind(&QueryServer::onUnknowMessage, this, _1, _2, _3))//注册一个无法识别的默认的回调
		//还可以注册一个解析出错的用户回调
		, codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3), NULL)
	{

		//注册收到确切protobuf类型的消息回调
		dispatcher_.registerMessageCallback<edwards::Query>(
			std::bind(&QueryServer::onQuery, this, _1, _2, _3));
		dispatcher_.registerMessageCallback<edwards::Answer>(
			std::bind(&QueryServer::onAnswer, this, _1, _2, _3));

		server_.setConnectionCallback(
			std::bind(&QueryServer::onConnection, this, _1));
		server_.setMessageCallback(
			std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));

	}
	~QueryServer() = default;

	void start()
	{
		server_.start();
	}


private:



	void onQuery(const muduo::net::TcpConnectionPtr& conn,
		const QueryPtr& message,
		muduo::Timestamp t)
	{
		LOG_DEBUG << "onQuery: \n" << message->GetTypeName()
			<< message->DebugString();

		message->PrintDebugString();


		edwards::Answer answer;
		answer.set_id(message->id());
		answer.set_questioner(message->questioner());
		answer.set_answerer("Your teacher");
		answer.add_solution("Jump!");
		answer.add_solution("Win!");

		messageToSend = &answer;

		codec_.send(conn, (*messageToSend));


	}

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
	/*		if (messageToSend)
				codec_.send(conn, *messageToSend);*/
		}
		else
		{
			//loop_->quit();
		}
	}



	EventLoop*			loop_;
	TcpServer			server_;
	ProtobufDispatcher	dispatcher_;
	ProtobufCodec		codec_;

};


int main(int argc, char* argv[])
{

	Logger::setLogLevel(Logger::TRACE);
	LOG_INFO << "pid = " << getpid();

	if (argc == 2)
	{
		uint16_t port = static_cast<uint16_t>(atoi(argv[1]));

		EventLoop loop;
		QueryServer server(&loop, InetAddress(port), "QueryServer");
		server.start();
		loop.loop();

	}
	else
	{
		printf("Usage: %s host_ip port [q|e]\n", argv[0]);
	}

	printf("\r\n=>>exit main.cpp \r\n");

}







