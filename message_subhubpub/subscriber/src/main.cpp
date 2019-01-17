
/**
* subscriber main implementation,main.cpp
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20181218
*/

#include "PubSubClient.h"

#include <muduo/base/Logging.h>
#include <muduo/base/ProcessInfo.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>

#include <boost/foreach.hpp>

#include <iostream>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;
using namespace pubhubsub;

EventLoop* g_loop = NULL;
std::vector<string> g_topics;
//string g_topic;
boost::weak_ptr<PubSubClient> g_tie;

void onSubscription(const string& topic, const string& content, Timestamp)
{
	printf("recv msg, topic: %s:[%s]\n", topic.c_str(), content.c_str());
}



void onConnection(const boost::shared_ptr<PubSubClient>& obj)
{
	g_tie = obj;

	boost::shared_ptr<PubSubClient> guard;
	guard = g_tie.lock();
	if ((guard) && (guard->connected()))//存在，且连接成功
	{
		LOG_DEBUG << "connection okay and sub topic. ";
		if (g_topics.size() > 0)
		{
			BOOST_FOREACH(auto topic, g_topics)
			{
				guard->subscribe(topic, onSubscription);//连接成功后再订阅主题，并设置回调
			}
		}
		else
		{
			LOG_DEBUG << "[no sub topic.]";
		}
	}
	else
	{
		LOG_WARN << "\r\n connection failure or remote has been disconnected. ";
		//g_loop->quit();//loop的生命周期必须长过client
		//g_loop = NULL;
		//当调用g_loop->quit()时，g_loop所指的eventloop会退出循环，并且释放栈上变量。
		//那么此时g_loop则是为空。
	}

}

void subByUser(const string& topic)
{
	boost::shared_ptr<PubSubClient> guard;
	guard = g_tie.lock();
	if ((guard) && (guard->connected()))//存在，且连接成功
	{
		guard->subscribe(topic, onSubscription);//连接成功后再订阅主题，并设置回调	
	}
	else
	{
		LOG_WARN << "connection failure or remote has been disconnected. ";
	}

}
void unsubByUser(const string& topic)
{
	boost::shared_ptr<PubSubClient> guard;
	guard = g_tie.lock();
	if ((guard) && (guard->connected()))//存在，且连接成功
	{
		guard->unsubscribe(topic);
	}
	else
	{
		LOG_WARN << "connection failure or remote has been disconnected. ";
	}
}

bool parseSubCommand(string* line, string* cmd, string* topic)
{
	int last = 0;
	auto found = line->find('<', last);
	if (found != string::npos)
	{
		(*cmd) = line->substr(0, found);
		auto colon_end = line->find('>');
		if (colon_end != string::npos)
		{
			(*topic) = line->substr(found + 1, colon_end - found - 1);
		}
		else
		{
			return false;
		}

	}
	else
	{
		return false;
	}
	
	LOG_DEBUG << "cmd :[" << *cmd << "]";
	LOG_DEBUG << "topic :[" << *topic << "]";

	return true;
		

}

int main(int argc, char* argv[])
{

	Logger::setLogLevel(Logger::TRACE);
	LOG_INFO << "pid = " << getpid();

	if (argc >= 3 )
	{
		string hostport = argv[1];
		auto colon = hostport.find(':');
		if (colon != string::npos)
		{
			string hostip = hostport.substr(0, colon);
			const uint16_t port = static_cast<uint16_t>(atoi(hostport.c_str() + colon + 1));

			string name = ProcessInfo::username() + "@" + ProcessInfo::hostname();
			name += ":" + ProcessInfo::pidString();
			string param = argv[2];
			if (param == "-")//动态的关注主题
			{
				LOG_DEBUG << "\r\n create loopThread.";

				EventLoopThread loopThread;
				g_loop = loopThread.startLoop();
				boost::shared_ptr<PubSubClient> sub_client_ptr(new PubSubClient(g_loop, InetAddress(hostip, port), name));
				sub_client_ptr->setConnectionCallback(onConnection);
				sub_client_ptr->start();//connect remote server


				LOG_DEBUG << "\r\n wait for inputing commad.";
				string line;
				//注意这里默认回车符停止读入, 按Ctrl + Z或键入EOF回车即可退出循环
				while (getline(std::cin, line))//手动订阅/取消主题
				{
					//parseString
					string cmd;
					string topic;
					auto ret = parseSubCommand(&line, &cmd, &topic);
					if (ret == true)
					{
						if (cmd == "sub")//sub<topic>
						{
							LOG_DEBUG << "send sub cmd";
							subByUser(topic);
						}
						else if (cmd == "unsub")//ubsub<topic>
						{
							LOG_DEBUG << "send unsub cmd";
							unsubByUser(topic);
						}
						else
						{
							LOG_WARN << "\r\nNOTE:input cmd error.";
						}
						//pub_client.publish(g_topic, line);
						LOG_WARN << "\r\n";
					}
					else
					{
						LOG_WARN << "\r\nNOTE:input format error.";
					}
				}

				LOG_DEBUG << "\r\nNOTE:exit process by user.";
				sub_client_ptr->stop();//loop的生命周期必须长过client
				//sub_client_ptr回收之前，先进行了延时.那么在这期间如果g_loop已经无效了，
				//延时结束回收sub_client_ptr时，在析构时还需要使用g_loop，那么就会出现内存泄漏。

				CurrentThread::sleepUsec(5000 * 1000);//5s
			}
			else
			{
				for (int i = 2; i < argc; ++i)
				{
					g_topics.push_back(argv[i]);
				}

				EventLoop loop;
				g_loop = &loop;

				boost::shared_ptr<PubSubClient> sub_client_ptr(new PubSubClient(g_loop, InetAddress(hostip, port), name));
				sub_client_ptr->setConnectionCallback(onConnection);
				//sub_client_ptr->subscribe(g_topic, onSubscription);//订阅主题，并设置主题回调
				sub_client_ptr->start();
				g_loop->loop();
			}
		}
		else
		{
			printf("Usage: %s hub_ip:port topic \n", argv[0]);
		}
	}
	else
	{
		printf("Usage: %s hub_ip:port topic \n", argv[0]);
	}




	printf("\r\n=>>exit main.cpp \r\n");

}








