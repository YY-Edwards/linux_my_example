
/**
* file upload server implementation,file_server_test.h
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180306
*/


#include "file_server/FileServer.h"

#include <malloc.h>
#include <sys/resource.h>

using namespace muduo;
using namespace muduo::net;

int main(int argc, char* argv[])
{

	Logger::setLogLevel(Logger::TRACE);
	LOG_INFO << "pid = " << getpid();

	if (argc == 3)
	{
		string host = argv[1];
		auto colon = host.find(':');
		if (colon != string::npos)
		{
			string ip = host.substr(0, colon);
			const uint16_t port = static_cast<uint16_t>(atoi(host.c_str() + colon + 1));
			const uint16_t maxConns = static_cast<uint16_t>(atoi(argv[2]));

			EventLoop loop;
			edwards::FileServer server(&loop, InetAddress(ip, port), maxConns, "FileServer");
			server.setThreadNumb(maxConns);
			server.start();
			loop.loop();
		}
	}
	else
	{
		printf("Usage: %s host_ip:port numb \n", argv[0]);
	}

	printf("\r\n=>>exit main.cpp \r\n");

}
