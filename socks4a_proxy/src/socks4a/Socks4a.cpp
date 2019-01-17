/**
* socks4a server main implementation,socks4a.cpp
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180116
*/

#include <muduo/base/LogFile.h>

#include "Tunnel.h"


#include <malloc.h>
#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>



EventLoop* g_eventLoop;
class Socks4aServer
{
public:
	Socks4aServer(EventLoop *loop,
		const InetAddress& localAddr//本地监听地址
		)
		:loop_(loop)
		, server_(loop, localAddr, "Socks4aServer")
	{
		server_.setConnectionCallback(
			std::bind(&Socks4aServer::onServerConnection, this, _1));
		server_.setMessageCallback(
			std::bind(&Socks4aServer::onServerMessage, this, _1, _2, _3));
	}
	~Socks4aServer()
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

		LOG_TRACE << "client " << conn->localAddress().toIpPort() << " -> Socks4aServer: "
			<< conn->peerAddress().toIpPort() << " is "
			<< (conn->connected() ? "UP" : "DOWN");

		if (conn->connected())//当有客户端主动连接Socks4aServer，并成功建立连接时。
		{
			conn->setTcpNoDelay(true);//禁用Nagle算法
		}
		else//客户端主动断开连接
		{
			if (tunnels_.find(conn->name()) != tunnels_.end())//如果有则删除
			{
				//直接下标操作必须得有上面的操作作为支撑。
				//因为如果不存在，直接下标操作会默认插入一个空指针。
				tunnels_[conn->name()]->disconnect();//断开隧道另一端的连接
				tunnels_.erase(conn->name());//删除隧道
			}

		}

	}
	void onServerMessage(const TcpConnectionPtr& conn,
						 Buffer* buf,
						 Timestamp receiveTime)
	{
		LOG_DEBUG << conn->name() << "send : " << buf->readableBytes() << " bytes.";

		//Socks4aServer作为server时，收到来自客户端的消息，需要解析标准的socks4a协议：
		//Client to SOCKS server: 
		//  field 1: SOCKS version number, 1 byte, must be 0x04 for this version
		//	field 2 : command code, 1 byte :
		//	0x01 = establish a TCP / IP stream connection
		//	0x02 = establish a TCP / IP port binding
		//	field 3 : port number, 2 bytes(in network byte order)
		//	field 4 : deliberate invalid IP address, 4 bytes, first three must be 0x00 and the last one must not be 0x00
		//	field 5 : the user ID string, variable length, terminated with a null(0x00)
		//	field 6 : the domain name of the host to contact, variable length, terminated with a null(0x00)


		//Server to SOCKS client :
		//field 1 : null byte
		//	field 2 : status, 1 byte :
		//	0x5A = request granted
		//	0x5B = request rejected or failed
		//	0x5C = request failed because client is not running identd(or not reachable from the server)
		//	0x5D = request failed because client's identd could not confirm the user ID string in the request
		//	field 3 : port number, 2 bytes(in network byte order)
		//	field 4 : IP address, 4 bytes(in network byte order)
		//隧道的server端收到client发送的数据
	
		if (tunnels_.find(conn->name()) == tunnels_.end())//确定是新建立，则需要首先解析socks4a协议
		{
			if (buf->readableBytes() > 8)//必须大于8（socks4 == 8 ）
			{
				const char* begin = buf->peek() + 8;
				const char* end = buf->peek() + buf->readableBytes();
				auto where = std::find(begin, end, '\0');
				if (where != end)//找到了；如果找不到就指向末尾
				{
					uint8_t ver = static_cast<uint8_t>(buf->peek()[0]);
					uint8_t cmd = static_cast<uint8_t>(buf->peek()[1]);
					const void* portPtr = buf->peek() + 2;
					const void* ipPtr = buf->peek() + 4;


					uint16_t port = *static_cast<const uint16_t*>(portPtr);
					uint32_t ip = *static_cast<const uint32_t*>(ipPtr);

					sockaddr_in addr;
					bzero(&addr, sizeof(addr));
					addr.sin_family = AF_INET;
					addr.sin_port = port;
					addr.sin_addr.s_addr = ip;//0.0.0.x

					bool socks4a = sockets::networkToHost32(ip) < 256;//指示是否需要进行域名解析
					bool okay = false;
					if (socks4a)//需要域名解析
					{
						//跳过user id string
						auto endOfDomainName = std::find((where + 1), end, '\0');
						if (endOfDomainName != end)
						{
							std::string hostName = (where + 1);
							where = endOfDomainName;//更新地址
							LOG_DEBUG << "Socks4a hostName: " << hostName;
							InetAddress tmp;
							//注意这里的域名解析，在真正的系统里应该使用异步的DSN解析
							if (InetAddress::resolve(hostName, &tmp))//解析域名：resolve hostname to IP address
							{
								addr.sin_addr.s_addr = tmp.ipNetEndian();//更新ip
								okay = true;
							}
						}
						else//格式有问题
						{
							LOG_DEBUG << "Socks4a format err! " ;
							return;
						}
					}
					else//已提供地址信息
					{
						okay = true;
					}

					InetAddress remoteServerAddr(addr);
					if (ver == 4 && cmd == 1 && okay)
					{
						TunnelPtr tunnel(new Tunnel(loop_, remoteServerAddr, conn));
						tunnel->setup();
						tunnel->connect();
						tunnels_[conn->name()] = tunnel;

						//根据处理后的偏移地址来偏移读指针
						buf->retrieveUntil(where + 1);//得益于字符串流处理

						//回复socks4a的client:请求准许
						char response[] = "\000\x5aUVWXYZ";
						memcpy(response + 2, &addr.sin_port, 2);
						memcpy(response + 4, &addr.sin_addr.s_addr, 4);
						conn->send(response, 8);
					}
					else//协议错误/域名解析错误
					{
						char response[] = "\000\x5bUVWXYZ";
						conn->send(response, 8);
						//并直接关闭连接
						conn->shutdown();
					}
				}
			}
			else
			{
				//等待粘包
			}
		}
		else//已通过socks4a协议，那么直接转发client的数据
		{
			if (!(conn->getContext().empty()))//162::隧道类里已经在当前TcpConnectionPtr绑定的对端client所建立的TcpConnection
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
	}

	EventLoop*							loop_;
	TcpServer							server_;
	//注意std::string 不要与 muduo::string混用，因编译器的问题可能会出现链接错误。
	//v2.0.0库已经把string全部替换成std::string?
	std::map<string, TunnelPtr>			tunnels_;
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

//通过client建立连接后由client的socks4a协议指定远端服务端地址
int main(int argc, char* argv[])
{

	muduo::Logger::setLogLevel(Logger::TRACE);
	LOG_INFO << "pid = " << getpid();


	if (argc < 1)
	{
		printf("Usage: %s [listen_port] \n", argv[0]);
	}
	else
	{

		{
			// set max virtual memory to 256MB.
			size_t kOneMB = 1024 * 1024;
			rlimit rl = { 256 * kOneMB, 256 * kOneMB };
			setrlimit(RLIMIT_AS, &rl);
		}

		const uint16_t listenPort = static_cast<uint16_t>(atoi(argv[1]));

		EventLoop loop;
		g_eventLoop = &loop;

		g_eventLoop->runEvery(5.0, memstat);//间隔查询内存分配


		Socks4aServer server(g_eventLoop, InetAddress(listenPort));
		server.start();
		loop.loop();
	}

	printf("\r\n=>>exit Socks4aServer.cpp \r\n");
}