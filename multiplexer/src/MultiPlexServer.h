
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
			//�Ƿ������߳�ģʽ
			server_.setThreadNum(numb);
		}


	private:


		void sendToClient(int id, const char* payload, int payloadLen);

		//send connection info to backend
		void sendBackendString(int id, const string& msg);
		//client send mgs to backend
		void sendBackenBuf(int id, Buffer* buf);


		//�ͻ�������ʹ�õĻص�
		void onClientConnection(const TcpConnectionPtr& conn);
		//void onClientMessage(const TcpConnectionPtr& conn,
		//					 Buffer* buf,
		//					 Timestamp receiveTime);

		//��̨������ʹ�õĻص�
		void onBackendConnection(const TcpConnectionPtr& conn);
		/*void onBackendMessage(const TcpConnectionPtr& conn,
							  Buffer* buf,
						      Timestamp receiveTime);*/

		TcpClient				backend_;
		TcpServer				server_;
		EventLoop*				loop_;

		MutexLock				mutex_;//

		DescriptorCodec			codec_;

		TcpConnectionPtr	    backendConn_;		//һ����̨
		std::map<int, TcpConnectionPtr> clientConns_;//����ͻ�
		std::queue<int>			availdIds_;			//��������ID��

	};

}

#endif  // __MULTIPLEX_SERVER_H