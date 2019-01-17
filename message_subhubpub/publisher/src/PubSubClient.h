

/**
* Pub/Sub Client header,PubSubClient.h
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20181218
*/

#ifndef __HUB_PUBSUB_CLIENT_H
#define __HUB_PUBSUB_CLIENT_H

#include <muduo/net/TcpClient.h>


namespace pubhubsub
{
	using muduo::string;
	using muduo::Timestamp;
	//using muduo::net::Buffer;
	//using muduo::net::TcpConnectionPtr;

	class PubSubClient : boost::noncopyable,//禁止拷贝
						//注意此时PubSubClient只能是heap中生成
						 public boost::enable_shared_from_this<PubSubClient>

	{
	public:


		//typedef boost::function<void(PubSubClient*)> ConnectinCallback;
		typedef boost::function<void(boost::shared_ptr<PubSubClient>)> ConnectinCallback;
		typedef boost::function<void(const string& topic,
									 const string& content,
									 Timestamp time)>SubscribeCallback;

		PubSubClient(muduo::net::EventLoop* loop,
					 const muduo::net::InetAddress& hubAddr,
					 const string& name);
		~PubSubClient();

		void start();
		void stop();
		bool connected() const;//函数里内容不允许改变
		void setConnectionCallback(const ConnectinCallback& cb)
		{
			userConnectionCallback_ = cb;
		}

		bool subscribe(const string& topic, const SubscribeCallback& cb);
		bool unsubscribe(const string& topic);
		bool publish(const string& topic, const string& content);

	private:

		void onConnection(const muduo::net::TcpConnectionPtr& conn);
		void onMessage(const muduo::net::TcpConnectionPtr& conn,
					   muduo::net::Buffer* buf,
					   Timestamp receiveTime);
		bool send(const string& msg);

		muduo::net::TcpConnectionPtr	conn_;
		muduo::net::TcpClient			client_;


		SubscribeCallback	subscribeCallback_;
		ConnectinCallback	userConnectionCallback_;

	};


}





#endif















