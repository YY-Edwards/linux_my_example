


/**
* Pub/Sub Client implementation,PubSubClient.cpp
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20181218
*/

#include "PubSubClient.h"
#include "Codec.h"

#include <muduo/base/Logging.h>
#include <boost/bind.hpp>

using namespace pubhubsub;
using namespace muduo;
using namespace muduo::net;

pubhubsub::PubSubClient::PubSubClient(muduo::net::EventLoop* loop,
									  const muduo::net::InetAddress& hubAddr,
									  const string& name)
									  :client_(loop, hubAddr, name)
{
	client_.setConnectionCallback(
		boost::bind(&PubSubClient::onConnection, this, _1));

	client_.setMessageCallback(
		boost::bind(&PubSubClient::onMessage, this, _1, _2, _3));

}

pubhubsub::PubSubClient::~PubSubClient()
{
	LOG_DEBUG << "deconstruct:[PubSubClient].";
}


void pubhubsub::PubSubClient::start()
{
	client_.connect();
	//默认开启掉线重连功能
	//client_.enableRetry();
}

void pubhubsub::PubSubClient::stop()
{

	client_.disconnect();//shutdown
}

bool pubhubsub::PubSubClient::connected() const
{
	//指针有效且已连接，则返回true
	return conn_ && conn_->connected();
}


void pubhubsub::PubSubClient::onConnection(const muduo::net::TcpConnectionPtr& conn) 
{
	if (conn->connected())
	{
		conn_ = conn;

		LOG_DEBUG << "connect server okay."
					<<" conn_,use_count: "<< conn_.use_count()
					<< " conn,use_count: " << conn.use_count();
	}
	else
	{
		conn_.reset();//计数指针减一
		LOG_DEBUG << "server disconnect!"
			<< " conn_,use_count: " << conn_.use_count()
			<< " conn,use_count: " << conn.use_count();
	}

	if (userConnectionCallback_)//用户使用的连接回调
	{
		//如果此时将this传递出去，那么得考虑PubSubClient生命周期的问题，
		//如果外层保存了PubSubClient对象指针时，并在某个时候调用了此对象，但是其实PubSubClient已经不在了，
		//继续使用this会导致内存泄漏。
		//将this转变成shared_ptr类型，即延长生命周期，则可解决。
		//connectionCallback_(this);
		userConnectionCallback_(shared_from_this());
	}


}
void pubhubsub::PubSubClient::onMessage(const muduo::net::TcpConnectionPtr& conn,
										muduo::net::Buffer* buf,
										Timestamp receiveTime)
{

	Parse_Result result = kSuccess;

	while (result == kSuccess)
	{
		string cmd;
		string topic;
		string content;
		result = parseMessage(buf, &cmd, &topic, &content);
		if (result == kSuccess)
		{
			if (cmd == "pub" && subscribeCallback_)//订阅了主题则回调
			{
				LOG_DEBUG << conn->name()
					<< "->publish:"
					<< topic << ", "
					<< "content:[" << content << "].";

				subscribeCallback_(topic, content, receiveTime);
			}
			else
			{
				LOG_DEBUG << conn->name()
					<< "->cmd no support!";

				//conn->shutdown();
				//result = kError;
			}

		}
		else if (result == kError)
		{
			LOG_DEBUG << conn->name()
				<< "protocol err!"
				<< "shutdown.";
			conn->shutdown();
			//这里只是关闭了写操作
			//如果对端故意不关闭，则考虑是否开启手动关闭
			//conn->forceClose();
		}
		//kContinue,退出，等待下一次接收直至完整数据包
	}


}


bool pubhubsub::PubSubClient::subscribe(const string& topic, const SubscribeCallback& cb)
{
	//可以每一个主题绑定不同的回调	
	subscribeCallback_ = cb;

	string message = "sub " + topic + "\r\n";

	return send(message);
}


bool pubhubsub::PubSubClient::unsubscribe(const string& topic)
{

	string message = "unsub " + topic + "\r\n";

	return send(message);
}

bool pubhubsub::PubSubClient::publish(const string& topic, const string& content)
{

	string message = "pub " + topic + "\r\n" + content + "\r\n";

	return send(message);
}




bool pubhubsub::PubSubClient::send(const string& msg)
{
	if (connected())
	{
		conn_->send(msg);
		return true;
	}
	else
	{
		LOG_DEBUG << "client connect fail";
		return false;
	}


}


