
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
	if ((guard) && (guard->connected()))//���ڣ������ӳɹ�
	{
		LOG_DEBUG << "connection okay and sub topic. ";
		if (g_topics.size() > 0)
		{
			BOOST_FOREACH(auto topic, g_topics)
			{
				guard->subscribe(topic, onSubscription);//���ӳɹ����ٶ������⣬�����ûص�
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
		//g_loop->quit();//loop���������ڱ��볤��client
		//g_loop = NULL;
		//������g_loop->quit()ʱ��g_loop��ָ��eventloop���˳�ѭ���������ͷ�ջ�ϱ�����
		//��ô��ʱg_loop����Ϊ�ա�
	}

}

void subByUser(const string& topic)
{
	boost::shared_ptr<PubSubClient> guard;
	guard = g_tie.lock();
	if ((guard) && (guard->connected()))//���ڣ������ӳɹ�
	{
		guard->subscribe(topic, onSubscription);//���ӳɹ����ٶ������⣬�����ûص�	
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
	if ((guard) && (guard->connected()))//���ڣ������ӳɹ�
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
			if (param == "-")//��̬�Ĺ�ע����
			{
				LOG_DEBUG << "\r\n create loopThread.";

				EventLoopThread loopThread;
				g_loop = loopThread.startLoop();
				boost::shared_ptr<PubSubClient> sub_client_ptr(new PubSubClient(g_loop, InetAddress(hostip, port), name));
				sub_client_ptr->setConnectionCallback(onConnection);
				sub_client_ptr->start();//connect remote server


				LOG_DEBUG << "\r\n wait for inputing commad.";
				string line;
				//ע������Ĭ�ϻس���ֹͣ����, ��Ctrl + Z�����EOF�س������˳�ѭ��
				while (getline(std::cin, line))//�ֶ�����/ȡ������
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
				sub_client_ptr->stop();//loop���������ڱ��볤��client
				//sub_client_ptr����֮ǰ���Ƚ�������ʱ.��ô�����ڼ����g_loop�Ѿ���Ч�ˣ�
				//��ʱ��������sub_client_ptrʱ��������ʱ����Ҫʹ��g_loop����ô�ͻ�����ڴ�й©��

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
				//sub_client_ptr->subscribe(g_topic, onSubscription);//�������⣬����������ص�
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








