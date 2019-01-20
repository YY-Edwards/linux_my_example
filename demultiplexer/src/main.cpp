
/**
* Demux server main implementation,main.cpp
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
* @revision time :20180118
*/

#include <muduo/base/LogFile.h>
#include <muduo/net/EventLoopThread.h>


#include "DemuxServer.h"



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
		string socksAddr = argv[2];
		auto colon = socksAddr.find(':');
		if (colon != std::string::npos)
		{
			string Ip = socksAddr.substr(0, colon);
			const uint16_t Port = static_cast<uint16_t>(atoi(socksAddr.c_str() + colon + 1));

			EventLoop loop;
			g_loop = &loop;
			DemuxServer server(g_loop, 
							   InetAddress(listenPort), 
							   InetAddress(Ip, Port));
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
		printf("Usage: %s [listen_port] [socks_ip:socks_port]  \n", argv[0]);
	}




}


