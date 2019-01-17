/**
* multiplexer server main implementation,main.cpp
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180107
*/

#include <muduo/base/LogFile.h>
#include <muduo/net/EventLoopThread.h>


#include "MultiPlexServer.h"



#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>

EventLoop* g_loop = NULL;




int main(int argc, char* argv[])
{

	muduo::Logger::setLogLevel(Logger::TRACE);
	LOG_INFO << "pid = " << getpid();
	if (argc > 3)
	{
		const uint16_t listenPort = static_cast<uint16_t>(atoi(argv[1]));
		string backendAddr = argv[2];
		auto colon = backendAddr.find(':');
		if (colon != std::string::npos)
		{
			string backendIp = backendAddr.substr(0, colon);
			const uint16_t backendPort = static_cast<uint16_t>(atoi(backendAddr.c_str() + colon + 1));

			EventLoop loop;
			g_loop = &loop;
			multiplexer::MultiPlexServer server(g_loop,
												InetAddress(listenPort),
												InetAddress(backendIp, backendPort));

			server.setThreadNumb(atoi(argv[3]));
			server.start();
			loop.loop();
		}
		else
		{
			printf("input error format.");
		}
	}
	else
	{
		printf("please input correct params: \r\n");
		printf("Usage: %s [listen_port] [back_ip:back_port] [thread_numb] \n", argv[0]);
	}
}



















