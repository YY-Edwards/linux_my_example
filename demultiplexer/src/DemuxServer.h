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


//DeMultiplexServer的功能需求（远端1/N:M的转发服务）如下：
//接收MultiplexServer合并起来的连接，即允许外部的一个或者多个客户端连接接入，
//并将1/N个TCP数据流中的每一个单独对待，每一个客户端拆解其中的连接，
//并分解为一个个单独的请求，然后再连接远端的socks4a服务端或者其他服务端。
//外部与本服务（backend）协议格式如下：
//header + content
//header:len[1] + id[2];
//len:a byte sum of content;
//本服务可以据此区分数据来源，对内数据流如下：
//len id_lo id_hi content...      len id_lo id_hi content...   len id_lo id_hi content...  
//外部与本服务间还可以通过特殊连接（id==0）用\r\n分割的文本协议沟通，本服务据此可以控制N:1连接服务：比如强制断开某个链接；
//获得新接入或离开的信号。
typedef std::shared_ptr<TcpClient> TcpClientPtr;

struct Entry
{
	int					connId;
	TcpClientPtr		client;
	TcpConnectionPtr	connection;
	Buffer				pending;
};

//目前测试，设定外部一对一连接
class DemuxServer : muduo::noncopyable
{



public:

	DemuxServer(EventLoop* loop,
				InetAddress& listenAddr,
				InetAddress& serverAddr);
	~DemuxServer();

	void start();


private:

	//与本服务端建立连接客户端的回调
	void onServerConnection(const TcpConnectionPtr& conn);
	//客户端发到本服务端的消息回调
	void onServerMessage(const TcpConnectionPtr& conn,
						 Buffer* buf,
						 Timestamp receiveTime);

	void doInnerCommand(const std::string& cmd);
	void sendPacketToServer(uint16_t connId, Buffer* buf);


	//与Socks服务建立连接的服务服务端回调
	void onSocksConnection(const uint16_t connId, const TcpConnectionPtr& conn);
	//Socks发到本k客户端端的消息回调
	void onSocksMessage(const uint16_t connId, 
						const TcpConnectionPtr& conn,
						Buffer* buf,
						Timestamp receiveTime);

	void setFrontMaxConns(uint16_t numb);
	void setFrontListenAction(uint16_t action);


	EventLoop*				loop_;
	InetAddress				socksAddr_;
	TcpServer				server_;
	TcpConnectionPtr		serverConn_;
	std::map<int, Entry>	socksConns_;


};





#endif  // __DEMUX_SERVER_H



