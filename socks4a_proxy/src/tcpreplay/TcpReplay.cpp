/**
* tcpreplay server main implementation,tcpreplay.cpp
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180107
*/

#include <muduo/base/LogFile.h>

//#include <boost/make_shared.hpp>

#include "Tunnel.h"


#include <malloc.h>
#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>



EventLoop* g_eventLoop;
class TcpReplay
{
public:
	TcpReplay(EventLoop *loop,
		const InetAddress& localAddr,//���ؼ�����ַ
		const InetAddress& remoteAddr//Զ����Ҫ���ӵķ���˵ĵ�ַ
		)
		:loop_(loop)
		, remoteAddr_(remoteAddr)
		, server_(loop, localAddr, "TcpReplay")
	{
		server_.setConnectionCallback(
			std::bind(&TcpReplay::onServerConnection, this, _1));
			//boost::bind(&TcpReplay::onServerConnection, this, _1));
		server_.setMessageCallback(
			std::bind(&TcpReplay::onServerMessage, this, _1, _2, _3));
			//boost::bind(&TcpReplay::onServerMessage, this, _1, _2, _3));
	}
	~TcpReplay()
	{
		LOG_TRACE;
	}


	void start()
	{
		server_.start();//start accept
	}


private:


	void onServerConnection(const TcpConnectionPtr& conn)
	{

		LOG_TRACE << "client " << conn->localAddress().toIpPort() << " -> TcpReplay: "
			<< conn->peerAddress().toIpPort() << " is "
			<< (conn->connected() ? "UP" : "DOWN");

		if (conn->connected())//���пͻ�����������TcpReplay�����ɹ���������ʱ��
		{
			conn->setTcpNoDelay(true);//����Nagle�㷨
			conn->stopRead();
			//����Tunnel
			//tunnel_.reset(new Tunnel(loop_, remoteAddr_, conn));
			//tunnel_ = boost::make_shared<Tunnel>(loop_, remoteAddr_, conn);
			//�������ӹ���һ��һ�������
			//TunnelPtr tunnel = boost::make_shared<Tunnel>(loop_, remoteAddr_, conn);
			//TunnelPtr tunnel(new Tunnel(loop_, remoteAddr_, conn));
			TunnelPtr tunnel = std::make_shared<Tunnel>(loop_, remoteAddr_, conn);

			if (tunnel)
			{
				tunnel->setup();
				tunnel->connect();//��������
				//ÿһ�����Ӷ�Ӧһ��һ���µ����
				 tunnels_[conn->name()] = tunnel;//�����������
			}
			else
			{
				//abort();
			}
		}
		else//�ͻ��������Ͽ�����
		{
			assert(tunnels_.find(conn->name()) != tunnels_.end());//�϶�����
			//ֱ���±���������������Ĳ�����Ϊ֧�š�
			//��Ϊ��������ڣ�ֱ���±������Ĭ�ϲ���һ����ָ�롣
			tunnels_[conn->name()]->disconnect();//�Ͽ������һ�˵�����
			tunnels_.erase(conn->name());//ɾ�����

		}

	}
	void onServerMessage(const TcpConnectionPtr& conn,
		Buffer* buf,
		Timestamp receiveTime)
	{
		LOG_DEBUG << conn->name() << "send :" << buf->readableBytes() << " bytes.";

		//TcpReplay��Ϊserverʱ���յ����Կͻ��˵���Ϣ
		//�����server���յ�client���͵�����

		if (!(conn->getContext().empty()))//��������Ѿ��ڵ�ǰTcpConnectionPtr�󶨵ĶԶ�client��������TcpConnection
		{
			//��conn���boost::any context_ʵ�崫�ݵ���ֵ����
			const TcpConnectionPtr& clientConn
				= boost::any_cast<const TcpConnectionPtr&>(conn->getContext());
			clientConn->send(buf);
		}
		else
		{
			//һ����ƫ��buf��ָ��
			buf->retrieveAll();
			LOG_DEBUG << conn->name() << " 's Tunnel has not been established! ";

		}

	}

	EventLoop*							loop_; 
	TcpServer							server_;
	//ע��std::string ��Ҫ�� muduo::string���ã����������������ܻ�������Ӵ���
	std::map<string, TunnelPtr>			tunnels_;
	//TunnelPtr							tunnel_;
	//boost::shared_ptr<Tunnel>			tunnel_;
	InetAddress							remoteAddr_;
};


void memstat()
{
	//valgrindҲ��й¶��鹤�ߣ��Ƚ�ȫ��

	//ͳ�Ʊ����̾�����ڴ�ʹ���������ȷ���ֽ�

	//Arena 0://��һ��arena��ÿ���̷߳���һ��arena��������ֻ��һ���߳�
	//system bytes     =     135168		//���̴߳Ӳ���ϵͳ��õĶ�̬�ڴ棬������132KB
	//in use bytes     =       1152		//���߳���ʹ�õĶ�̬�ڴ棬1152�ֽ�
	//Total (incl. mmap):				//�ܵ�ʹ������������߳�ʹ�ö�̬�ڴ���ۼ�ֵ
	//system bytes     =     135168		//�����̴Ӳ���ϵͳ��õĶ�̬�ڴ棬������132KB
	//in use bytes     =       1152		//��������ʹ�õĶ�̬�ڴ棬1152�ֽ�
	//max mmap regions =          0		//��һ�������ڴ泬��128KB��32λ����ϵͳ����1MB��64λ����ϵͳ��ʱ��������mmap��������ͳ��ʹ��mmap����ĸ���
	//max mmap bytes   =          0		//mmap�����Ӧ�ڴ��С


	malloc_stats();//���Լ���ڴ�й¶
}

//�ֶ�ָ������˵�ַ
int main(int argc, char* argv[])
{

	muduo::Logger::setLogLevel(Logger::TRACE);
	LOG_INFO << "pid = " << getpid();


	if (argc < 4)
	{
		printf("Usage: %s [host_ip] [port] [listen_port] \n", argv[0]);
	}
	else
	{

		{
			// set max virtual memory to 256MB.
			size_t kOneMB = 1024 * 1024;
			rlimit rl = { 256 * kOneMB, 256 * kOneMB };
			setrlimit(RLIMIT_AS, &rl);
		}

		const uint16_t listenPort = static_cast<uint16_t>(atoi(argv[3]));
		const char* ip = argv[1];
		const uint16_t port = static_cast<uint16_t>(atoi(argv[2]));

		EventLoop loop;
		g_eventLoop = &loop;

		g_eventLoop->runEvery(5.0, memstat);//�����ѯ�ڴ����


		TcpReplay replay(g_eventLoop, InetAddress(listenPort), InetAddress(ip, port));
		replay.start();
		loop.loop();
	}

	printf("\r\n=>>exit TcpReplay.cpp \r\n");
}









