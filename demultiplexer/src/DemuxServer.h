/**
* DeMultiPlex Server header,DemuxServer.h
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180117
*/

#ifndef __DEMUX_SERVER_H
#define __DEMUX_SERVER_H

#include <muduo/base/Atomic.h>
#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/TcpClient.h>


#include <map>
#include <set>
#include <stdio.h>
#include <queue>


using namespace::muduo;
using namespace::muduo::net;


//DeMultiplexServer�Ĺ�������Զ��1/N:M��ת���������£�
//����MultiplexServer�ϲ����������ӣ��������ⲿ��һ�����߶���ͻ������ӽ��룬
//����1/N��TCP�������е�ÿһ�������Դ���ÿһ���ͻ��˲�����е����ӣ�
//���ֽ�Ϊһ��������������Ȼ��������Զ�˵�socks4a����˻�����������ˡ�
//�ⲿ�뱾����backend��Э���ʽ���£�
//header + content
//header:len[1] + id[2];
//len:a byte sum of content;
//��������Ծݴ�����������Դ���������������£�
//len id_lo id_hi content...      len id_lo id_hi content...   len id_lo id_hi content...  
//�ⲿ�뱾����仹����ͨ���������ӣ�id==0����\r\n�ָ���ı�Э�鹵ͨ��������ݴ˿��Կ���N:1���ӷ��񣺱���ǿ�ƶϿ�ĳ�����ӣ�
//����½�����뿪���źš�
typedef std::shared_ptr<TcpClient> TcpClientPtr;

struct Entry
{
	int					connId;
	TcpClientPtr		client;
	TcpConnectionPtr	connection;
	Buffer				pending;
};

//Ŀǰ���ԣ��趨�ⲿһ��һ����
class DemuxServer : muduo::noncopyable
{



public:

	DemuxServer(EventLoop* loop,
				InetAddress& listenAddr,
				InetAddress& serverAddr);
	~DemuxServer();

	void start();


private:

	//�뱾����˽������ӿͻ��˵Ļص�
	void onServerConnection(const TcpConnectionPtr& conn);
	//�ͻ��˷���������˵���Ϣ�ص�
	void onServerMessage(const TcpConnectionPtr& conn,
						 Buffer* buf,
						 Timestamp receiveTime);

	//��Socks���������ӵķ������˻ص�
	void onSocksConnection(const TcpConnectionPtr& conn);
	//Socks������k�ͻ��˶˵���Ϣ�ص�
	void onSocksMessage(const TcpConnectionPtr& conn,
						Buffer* buf,
						Timestamp receiveTime);


	EventLoop*				loop_;
	InetAddress				socksAddr_;
	TcpServer				server_;
	TcpConnectionPtr		serverConn_;
	std::map<int, Entry>	socksConns_;


};





#endif  // __DEMUX_SERVER_H



