
/**
* MultiPlex Server header,MultiPlexServer.h
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180107
*/

#ifndef __MULTIPLEX_SERVER_H
#define __MULTIPLEX_SERVER_H

#include <muduo/base/Atomic.h>
#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/ThreadLocalSingleton.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/TcpClient.h>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

#include <map>
#include <set>
#include <stdio.h>
#include <queue>

#include "Codec.h"
//#include <algorithm>

using namespace::muduo;
using namespace::muduo::net;


//MutilplexServer的功能需求（本地N:1连接转发服务）如下：
//允许外部的多个客户端连接接入，并将N个TCP数据流合成一个，然后转发到后台（backend）。
//外部与本服务直接数据透传，后台与本服务的协议格式如下：
//header + content
//header:len[1] + id[2];
//len:a byte sum of content;
//后台可以据此区分数据来源，对内数据流如下：
//len id_lo id_hi content...      len id_lo id_hi content...   len id_lo id_hi content...  
//后台与本服务间还可以通过特殊连接（id==0）用\r\n分割的文本协议沟通，后台据此可以控制本连接服务：比如强制断开某个链接；
//获得新接入或离开的信号。


namespace multiplexer
{

	const int kMaxConns = 10;//65535

	class MultiPlexServer : boost::noncopyable
	{

	public:

		MultiPlexServer(muduo::net::EventLoop* loop,
						const muduo::net::InetAddress& listenAddr,
						const muduo::net::InetAddress& backendAddr);
		~MultiPlexServer()=default;

		void start();
		void setThreadNumb(int numb)
		{
			//是否开启多线程模式
			server_.setThreadNum(numb);
		}


	private:


		void sendToClient(int id, const char* payload, int payloadLen);

		//send connection info to backend
		void sendBackendString(int id, const string& msg);
		//client send mgs to backend
		void sendBackenBuf(int id, Buffer* buf);


		//客户连接所使用的回调
		void onClientConnection(const TcpConnectionPtr& conn);
		//void onClientMessage(const TcpConnectionPtr& conn,
		//					 Buffer* buf,
		//					 Timestamp receiveTime);

		//后台连接所使用的回调
		void onBackendConnection(const TcpConnectionPtr& conn);
		/*void onBackendMessage(const TcpConnectionPtr& conn,
							  Buffer* buf,
						      Timestamp receiveTime);*/

		TcpClient				backend_;
		TcpServer				server_;
		EventLoop*				loop_;

		MutexLock				mutex_;//

		DescriptorCodec			codec_;

		TcpConnectionPtr	    backendConn_;		//一个后台
		std::map<int, TcpConnectionPtr> clientConns_;//多个客户
		std::queue<int>			availdIds_;			//存放允许的ID号

	};

}

#endif  // __MULTIPLEX_SERVER_H