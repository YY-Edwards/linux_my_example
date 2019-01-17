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
		const InetAddress& localAddr//���ؼ�����ַ
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

		if (conn->connected())//���пͻ�����������Socks4aServer�����ɹ���������ʱ��
		{
			conn->setTcpNoDelay(true);//����Nagle�㷨
		}
		else//�ͻ��������Ͽ�����
		{
			if (tunnels_.find(conn->name()) != tunnels_.end())//�������ɾ��
			{
				//ֱ���±���������������Ĳ�����Ϊ֧�š�
				//��Ϊ��������ڣ�ֱ���±������Ĭ�ϲ���һ����ָ�롣
				tunnels_[conn->name()]->disconnect();//�Ͽ������һ�˵�����
				tunnels_.erase(conn->name());//ɾ�����
			}

		}

	}
	void onServerMessage(const TcpConnectionPtr& conn,
						 Buffer* buf,
						 Timestamp receiveTime)
	{
		LOG_DEBUG << conn->name() << "send : " << buf->readableBytes() << " bytes.";

		//Socks4aServer��Ϊserverʱ���յ����Կͻ��˵���Ϣ����Ҫ������׼��socks4aЭ�飺
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
		//�����server���յ�client���͵�����
	
		if (tunnels_.find(conn->name()) == tunnels_.end())//ȷ�����½���������Ҫ���Ƚ���socks4aЭ��
		{
			if (buf->readableBytes() > 8)//�������8��socks4 == 8 ��
			{
				const char* begin = buf->peek() + 8;
				const char* end = buf->peek() + buf->readableBytes();
				auto where = std::find(begin, end, '\0');
				if (where != end)//�ҵ��ˣ�����Ҳ�����ָ��ĩβ
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

					bool socks4a = sockets::networkToHost32(ip) < 256;//ָʾ�Ƿ���Ҫ������������
					bool okay = false;
					if (socks4a)//��Ҫ��������
					{
						//����user id string
						auto endOfDomainName = std::find((where + 1), end, '\0');
						if (endOfDomainName != end)
						{
							std::string hostName = (where + 1);
							where = endOfDomainName;//���µ�ַ
							LOG_DEBUG << "Socks4a hostName: " << hostName;
							InetAddress tmp;
							//ע�������������������������ϵͳ��Ӧ��ʹ���첽��DSN����
							if (InetAddress::resolve(hostName, &tmp))//����������resolve hostname to IP address
							{
								addr.sin_addr.s_addr = tmp.ipNetEndian();//����ip
								okay = true;
							}
						}
						else//��ʽ������
						{
							LOG_DEBUG << "Socks4a format err! " ;
							return;
						}
					}
					else//���ṩ��ַ��Ϣ
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

						//���ݴ�����ƫ�Ƶ�ַ��ƫ�ƶ�ָ��
						buf->retrieveUntil(where + 1);//�������ַ���������

						//�ظ�socks4a��client:����׼��
						char response[] = "\000\x5aUVWXYZ";
						memcpy(response + 2, &addr.sin_port, 2);
						memcpy(response + 4, &addr.sin_addr.s_addr, 4);
						conn->send(response, 8);
					}
					else//Э�����/������������
					{
						char response[] = "\000\x5bUVWXYZ";
						conn->send(response, 8);
						//��ֱ�ӹر�����
						conn->shutdown();
					}
				}
			}
			else
			{
				//�ȴ�ճ��
			}
		}
		else//��ͨ��socks4aЭ�飬��ôֱ��ת��client������
		{
			if (!(conn->getContext().empty()))//162::��������Ѿ��ڵ�ǰTcpConnectionPtr�󶨵ĶԶ�client��������TcpConnection
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
	}

	EventLoop*							loop_;
	TcpServer							server_;
	//ע��std::string ��Ҫ�� muduo::string���ã����������������ܻ�������Ӵ���
	//v2.0.0���Ѿ���stringȫ���滻��std::string?
	std::map<string, TunnelPtr>			tunnels_;
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

//ͨ��client�������Ӻ���client��socks4aЭ��ָ��Զ�˷���˵�ַ
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

		g_eventLoop->runEvery(5.0, memstat);//�����ѯ�ڴ����


		Socks4aServer server(g_eventLoop, InetAddress(listenPort));
		server.start();
		loop.loop();
	}

	printf("\r\n=>>exit Socks4aServer.cpp \r\n");
}