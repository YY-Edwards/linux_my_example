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
//	// scoped_ptr<T> ����operator->������LogFile::append(), 
//	// ��ӵ���File::append(); ��� ::fwrite_unlocked(fp_);
//}
//
//void flushFunc()
//{
//	g_logFile->flush();
//	// scoped_ptr<T> ����operator->������LogFile::flush(), 
//	//��ӵ���File::flush()�����::fflush(fp_);
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

		//logger������ļ�������������
		//char name[64] = { 0 };
		//strncpy(name, argv[0], sizeof name - 1);
		//include <libgen.h> :basename
		//�����ļ�������ת��С���̰߳�ȫ��flush���������
		//g_logFile.reset(new muduo::LogFile(::basename(name), 200 * 1000, true, 3, 3));
		//���ûص���ԭʼ,��bind��
		//muduo::Logger::setOutput(outputFunc);
		//muduo::Logger::setFlush(flushFunc);


		EventLoop loop;

		g_loop = &loop;

		pubhubsub::HubServer server(&loop, InetAddress(port));
		if (argc > 2)
		{
			//���߳�ģʽ
			server.setThreadNumb(atoi(argv[2]));
		}
		server.start();

		loop.runAfter(15, threadFuc);

		loop.loop();

		/*ע��:
			�����ioloop��������һ���̴߳����ģ�
			��server�������ڵ�ǰ���̴߳����ġ�
			��ʱ��loop��ַ������server��

			֮�������߳����ֵ���server.start()�����ŵ���TcpServer.start()�ӿ�;
			Ȼ�����EventLoopThreadPool::start(),����ӿ����ͨ��baseLoop_->assertInLoopThread()�ӿ��ж��Ƿ�����ioloop�߳���
			���е��õġ�
			����ͻ���ΪΥ��one loop per threadԭ����жϡ����������̴߳���loop����,
			ȴ��ͼ�����̵߳�����EventLoop::assertInLoopThread(),����������ʧЧ���쳣��ֹ��
			��TcpServer��Ҫת�Ƶ�ioloop��ȥִ�У�ò�ƺ����ѡ�

			������һ��client�У�����Ҳ�������̴߳���loop��
			Ȼ�������̵߳���client.start()->TcpClinet.connect()->Connector.start();
			Ȼ����Connector::start()�ӿ��У�����ʹ��loop->runinloop(Connector::func)�ķ�ʽ��
			������ת�Ƶ�ioloop��ȥִ�С����ԣ�������ֿ��̵߳��÷Ǳ��̴߳�����loop����Ϊ��
		
		*/

		//EventLoopThread loopThread;
		//g_loop = loopThread.startLoop();
		//pubhubsub::HubServer server(g_loop, InetAddress(port));
		//if (argc > 2)
		//{
		//	//���߳�ģʽ
		//	server.setThreadNumb(atoi(argv[2]));
		//}

		//server.start();

		//string line;
		//while (getline(std::cin, line))//�ֶ��˳�
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