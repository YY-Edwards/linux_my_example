/**
* hub server main implementation,main.cpp
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20181218
*/

#include <muduo/base/LogFile.h>
#include <muduo/net/EventLoopThread.h>


#include "HubServer.h"



#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>

EventLoop* g_loop = NULL;

//
//boost::scoped_ptr<muduo::LogFile> g_logFile;
//
//void outputFunc(const char* msg, int len)
//{
//	g_logFile->append(msg, len);
//	// scoped_ptr<T> 重载operator->，调用LogFile::append(), 
//	// 间接调用File::append(); 最后 ::fwrite_unlocked(fp_);
//}
//
//void flushFunc()
//{
//	g_logFile->flush();
//	// scoped_ptr<T> 重载operator->，调用LogFile::flush(), 
//	//间接调用File::flush()，最后::fflush(fp_);
//}


void threadFuc(void)
{
	if (g_loop)
		g_loop->quit();
}

int main(int argc, char* argv[])
{
	
	muduo::Logger::setLogLevel(Logger::TRACE);
	LOG_INFO << "pid = " << getpid();

	if (argc > 1)
	{
		const uint16_t port = static_cast<uint16_t>(atoi(argv[1]));

		//logger输出到文件，按如下配置
		//char name[64] = { 0 };
		//strncpy(name, argv[0], sizeof name - 1);
		//include <libgen.h> :basename
		//设置文件名，翻转大小，线程安全，flush间隔，行数
		//g_logFile.reset(new muduo::LogFile(::basename(name), 200 * 1000, true, 3, 3));
		//设置回调（原始,非bind）
		//muduo::Logger::setOutput(outputFunc);
		//muduo::Logger::setFlush(flushFunc);


		EventLoop loop;

		g_loop = &loop;

		pubhubsub::HubServer server(&loop, InetAddress(port));
		if (argc > 2)
		{
			//多线程模式
			server.setThreadNumb(atoi(argv[2]));
		}
		server.start();

		loop.runAfter(15, threadFuc);

		loop.loop();

		/*注意:
			这里的ioloop是在另外一个线程创建的，
			而server对象是在当前主线程创建的。
			此时将loop地址传给了server。

			之后在主线程中又调用server.start()，接着调用TcpServer.start()接口;
			然后调用EventLoopThreadPool::start(),这个接口里会通过baseLoop_->assertInLoopThread()接口判断是否是在ioloop线程中
			进行调用的。
			这里就会因为违背one loop per thread原则而中断。即是在子线程创建loop对象,
			却试图在主线程调用了EventLoop::assertInLoopThread(),程序会因断言失效而异常终止。
			而TcpServer想要转移到ioloop中去执行，貌似很困难。

			而，另一端client中，首先也是在子线程创建loop，
			然后在主线程调用client.start()->TcpClinet.connect()->Connector.start();
			然后在Connector::start()接口中，作者使用loop->runinloop(Connector::func)的方式，
			将函数转移到ioloop中去执行。所以，不会出现跨线程调用非本线程创建的loop的行为。
		
		*/

		//EventLoopThread loopThread;
		//g_loop = loopThread.startLoop();
		//pubhubsub::HubServer server(g_loop, InetAddress(port));
		//if (argc > 2)
		//{
		//	//多线程模式
		//	server.setThreadNumb(atoi(argv[2]));
		//}

		//server.start();

		//string line;
		//while (getline(std::cin, line))//手动退出
		//{

		//}
		//LOG_DEBUG << "\r\nNOTE:exit process by user.";
		//g_loop->quit();
		//g_loop = NULL;
		//server
	}
	else
	{
		printf("Usage: %s hub_port [inspect_port]\n", argv[0]);
	}

	printf("\r\n=>>exit main.cpp \r\n");

	g_loop = NULL;
}