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
		const InetAddress& localAddr,//本地监听地址
		const InetAddress& remoteAddr//远端需要连接的服务端的地址
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

		if (conn->connected())//当有客户端主动连接TcpReplay，并成功建立连接时。
		{
			conn->setTcpNoDelay(true);//禁用Nagle算法
			conn->stopRead();
			//构造Tunnel
			//tunnel_.reset(new Tunnel(loop_, remoteAddr_, conn));
			//tunnel_ = boost::make_shared<Tunnel>(loop_, remoteAddr_, conn);
			//并发连接管理（一对一的隧道）
			//TunnelPtr tunnel = boost::make_shared<Tunnel>(loop_, remoteAddr_, conn);
			//TunnelPtr tunnel(new Tunnel(loop_, remoteAddr_, conn));
			TunnelPtr tunnel = std::make_shared<Tunnel>(loop_, remoteAddr_, conn);

			if (tunnel)
			{
				tunnel->setup();
				tunnel->connect();//启动连接
				//每一个连接对应一个一个新的隧道
				 tunnels_[conn->name()] = tunnel;//插入隧道表中
			}
			else
			{
				//abort();
			}
		}
		else//客户端主动断开连接
		{
			assert(tunnels_.find(conn->name()) != tunnels_.end());//肯定得有
			//直接下标操作必须得有上面的操作作为支撑。
			//因为如果不存在，直接下标操作会默认插入一个空指针。
			tunnels_[conn->name()]->disconnect();//断开隧道另一端的连接
			tunnels_.erase(conn->name());//删除隧道

		}

	}
	void onServerMessage(const TcpConnectionPtr& conn,
		Buffer* buf,
		Timestamp receiveTime)
	{
		LOG_DEBUG << conn->name() << "send :" << buf->readableBytes() << " bytes.";

		//TcpReplay作为server时，收到来自客户端的消息
		//隧道的server端收到client发送的数据

		if (!(conn->getContext().empty()))//隧道类里已经在当前TcpConnectionPtr绑定的对端client所建立的TcpConnection
		{
			//从conn里的boost::any context_实体传递的左值引用
			const TcpConnectionPtr& clientConn
				= boost::any_cast<const TcpConnectionPtr&>(conn->getContext());
			clientConn->send(buf);
		}
		else
		{
			//一定得偏移buf的指针
			buf->retrieveAll();
			LOG_DEBUG << conn->name() << " 's Tunnel has not been established! ";

		}

	}

	EventLoop*							loop_; 
	TcpServer							server_;
	//注意std::string 不要与 muduo::string混用，因编译器的问题可能会出现链接错误。
	std::map<string, TunnelPtr>			tunnels_;
	//TunnelPtr							tunnel_;
	//boost::shared_ptr<Tunnel>			tunnel_;
	InetAddress							remoteAddr_;
};


void memstat()
{
	//valgrind也是泄露检查工具，比较全面

	//统计本进程具体的内存使用情况，精确到字节

	//Arena 0://第一个arena（每个线程分配一个arena），这里只有一个线程
	//system bytes     =     135168		//本线程从操作系统获得的动态内存，这里是132KB
	//in use bytes     =       1152		//本线程在使用的动态内存，1152字节
	//Total (incl. mmap):				//总的使用情况，各个线程使用动态内存的累加值
	//system bytes     =     135168		//本进程从操作系统获得的动态内存，这里是132KB
	//in use bytes     =       1152		//本进程在使用的动态内存，1152字节
	//max mmap regions =          0		//当一次申请内存超过128KB（32位操作系统）或1MB（64位操作系统）时，会增加mmap区域，这里统计使用mmap区域的个数
	//max mmap bytes   =          0		//mmap区域对应内存大小


	malloc_stats();//用以检查内存泄露
}

//手动指定服务端地址
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

		g_eventLoop->runEvery(5.0, memstat);//间隔查询内存分配


		TcpReplay replay(g_eventLoop, InetAddress(listenPort), InetAddress(ip, port));
		replay.start();
		loop.loop();
	}

	printf("\r\n=>>exit TcpReplay.cpp \r\n");
}









