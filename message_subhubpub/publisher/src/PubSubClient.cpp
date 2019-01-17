


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
	//Ĭ�Ͽ���������������
	//client_.enableRetry();
}

void pubhubsub::PubSubClient::stop()
{

	client_.disconnect();//shutdown
}

bool pubhubsub::PubSubClient::connected() const
{
	//ָ����Ч�������ӣ��򷵻�true
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
		conn_.reset();//����ָ���һ
		LOG_DEBUG << "server disconnect!"
			<< " conn_,use_count: " << conn_.use_count()
			<< " conn,use_count: " << conn.use_count();
	}

	if (userConnectionCallback_)//�û�ʹ�õ����ӻص�
	{
		//�����ʱ��this���ݳ�ȥ����ô�ÿ���PubSubClient�������ڵ����⣬
		//�����㱣����PubSubClient����ָ��ʱ������ĳ��ʱ������˴˶��󣬵�����ʵPubSubClient�Ѿ������ˣ�
		//����ʹ��this�ᵼ���ڴ�й©��
		//��thisת���shared_ptr���ͣ����ӳ��������ڣ���ɽ����
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
			if (cmd == "pub" && subscribeCallback_)//������������ص�
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
			//����ֻ�ǹر���д����
			//����Զ˹��ⲻ�رգ������Ƿ����ֶ��ر�
			//conn->forceClose();
		}
		//kContinue,�˳����ȴ���һ�ν���ֱ���������ݰ�
	}


}


bool pubhubsub::PubSubClient::subscribe(const string& topic, const SubscribeCallback& cb)
{
	//����ÿһ������󶨲�ͬ�Ļص�	
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


