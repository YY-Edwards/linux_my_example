
/**
* publisher main implementation,main.cpp
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


#include <iostream>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;
using namespace pubhubsub;


EventLoop* g_loop = NULL;
string g_topic;
string g_content;
boost::weak_ptr<PubSubClient> g_tie;

void connection(const boost::shared_ptr<PubSubClient>& obj)
{
	g_tie = obj;
}

void timerPublisher()
{
	boost::shared_ptr<PubSubClient> guard;
	guard = g_tie.lock();
	if ((guard) && (guard->connected()))
	{
		guard->publish(g_topic, g_content);
		//guard->stop();
	}
	else
	{
		LOG_WARN << "connection failure or remote has been disconnected. ";
		//g_loop->quit();//��ǰ�˳����ᵼ���ڴ�й©����Ϊ�˳�ʱ��Ϊ����̫����ʱ��
	}

}



int main(int argc, char* argv[])
{

	Logger::setLogLevel(Logger::TRACE);
	LOG_INFO << "pid = " << getpid();

	if (argc == 4 )
	{
		string hostport = argv[1];
		auto colon = hostport.find(':');
		if (colon != string::npos)
		{
			string hostip = hostport.substr(0, colon);
			const uint16_t port = static_cast<uint16_t>(atoi(hostport.c_str() + colon + 1));
			g_topic = argv[2];
			g_content = argv[3];

			string name = ProcessInfo::username() + "@" + ProcessInfo::hostname();
			name += ":" + ProcessInfo::pidString();

			if (g_content == "-")//��̬�ķ������ݣ�Ҳ���Ը���Ϊ��̬���ⷢ��
			{
				EventLoopThread loopThread;
				g_loop = loopThread.startLoop();
				PubSubClient pub_client(g_loop, InetAddress(hostip, port), name);
				//pub_client.setConnectionCallback();
				pub_client.start();//connect remote server

				string line;
				//ע������Ĭ�ϻس���ֹͣ����, ��Ctrl + Z�����EOF�س������˳�ѭ��
				while (getline(std::cin, line))//�ֶ����Ʒ���
				{
					pub_client.publish(g_topic, line);
				}

				pub_client.stop();

				CurrentThread::sleepUsec(5000 * 1000);//5s
			}
			else
			{

				EventLoop loop;
				g_loop = &loop;

				//PubSubClient pub_client(g_loop, InetAddress(hostip, port), name);
				//pub_client.setConnectionCallback(connection);
				//pub_client.start();

				boost::shared_ptr<PubSubClient> pub_client_ptr(new PubSubClient(g_loop, InetAddress(hostip, port), name));
				pub_client_ptr->setConnectionCallback(connection);
				pub_client_ptr->start();
				g_loop->runEvery(5, timerPublisher);//��ʱ�������⣬����
				g_loop->loop();
			}
		}
		else
		{
			printf("Usage: %s hub_ip:port topic content\n", argv[0]);
		}
	}
	else
	{
		printf("Usage: %s hub_ip:port topic content\n"
				"Read contents from stdin:\n"
				"  %s hub_ip:port topic -\n", argv[0], argv[0]);
	}




	printf("\r\n=>>exit main.cpp \r\n");

}








