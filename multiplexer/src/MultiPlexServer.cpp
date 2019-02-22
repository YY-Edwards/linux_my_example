/**
* MultiPlex Server implementation,MultiPlexServer.cpp
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180107
*/

#include "MultiPlexServer.h"

using namespace multiplexer;

multiplexer::MultiPlexServer::MultiPlexServer(muduo::net::EventLoop* loop,
											  const muduo::net::InetAddress& listenAddr,
											  const muduo::net::InetAddress& backendAddr)
	:loop_(loop)
	, server_(loop, listenAddr, "MultiPlexServer")
	, backend_(loop, backendAddr, "MultiPlexBackend")
	, codec_(boost::bind(&MultiPlexServer::sendBackenBuf, this, _1, _2),
		boost::bind(&MultiPlexServer::sendToClient, this, _1, _2, _3))
{

	server_.setConnectionCallback(
		boost::bind(&MultiPlexServer::onClientConnection, this, _1));
	server_.setMessageCallback(
		boost::bind(&DescriptorCodec::onClientMessage, &codec_, _1, _2, _3));//parse client message

	/*server_.setMessageCallback(
		boost::bind(&MultiPlexServer::onClientMessage, this, _1, _2, _3));*/



	backend_.setConnectionCallback(
		boost::bind(&MultiPlexServer::onBackendConnection, this, _1));
	backend_.setMessageCallback(
		boost::bind(&DescriptorCodec::onBackendMessage, &codec_, _1, _2, _3));//parse backend message
	//backend_.setMessageCallback(
	//	boost::bind(&MultiPlexServer::onBackendMessage, this, _1, _2, _3));
	//默认开启掉线重连功能
	backend_.enableRetry();//开启断开自动重连功能


	loop_->runEvery(10.0, boost::bind(&DescriptorCodec::printStatistics, &codec_));

}


void multiplexer::MultiPlexServer::start()
{
	//设置各个ioloop的前置回调
	//server_.setThreadInitCallback(boost::bind(&MultiPlexServer::threadInitFunc, this, _1));
	//这里可以调整为由backend来控制监听端口的开启与关闭，以及外部连接的上限
	backend_.connect();
	server_.start();
	
}


void multiplexer::MultiPlexServer::onBackendConnection(const TcpConnectionPtr& conn)
{
	LOG_TRACE << "Backend " << conn->localAddress().toIpPort() << " -> "
		<< conn->peerAddress().toIpPort() << " is "
		<< (conn->connected() ? "UP" : "DOWN");

	//boost::ptr_vector<TcpConnection>;
	std::vector<TcpConnectionPtr> connsToDestroy;
	if (conn->connected())
	{
		MutexLockGuard lock(mutex_);
		backendConn_ = conn;
		assert(availdIds_.empty());
		for (int i = 1; i <= kMaxConns; i++)//创建预留的ID
		{
			availdIds_.push(i);
		}
	}
	else//断开
	{
		MutexLockGuard lock(mutex_);
		backendConn_.reset();
		BOOST_FOREACH(auto it, clientConns_)//取出所有连接对象
		{	
			//貌似对指针类型的标准库对象并不需要这么做.
			connsToDestroy.emplace_back(std::move(it.second));//测试看看会不会出现析构，这里引用计数应该加1了
			//connsToDestroy.push_back(it.second);
			//emplace_back里可以传入右值，而it.second是左值
			//std::move(it.second)可以将左值转移为右值。即右值绑定在左值上。
			//(it.second)->shutdown();//stop write，这样直接操作应该不会受生命周期的影响，因为全程上锁了
			//但是因为需要进而操作shutdown肯能效率不是很高
		}
		clientConns_.clear();
		while (!availdIds_.empty())
		{
			availdIds_.pop();
		}
	}

	BOOST_FOREACH(auto it, connsToDestroy)
	{
		it->shutdown();//会引发readfd == 0;然后回调onClientConnection(disconnected)=>erase。
		LOG_DEBUG;
	}
}

void multiplexer::MultiPlexServer::onClientConnection(const TcpConnectionPtr& conn)
{
	LOG_TRACE << conn->localAddress().toIpPort()
		<< " -> "
		<< conn->peerAddress().toIpPort()
		<< " is "
		<< (conn->connected() ? "UP" : "DOWN");

	if (conn->connected())
	{	
		int id = -1;
		{
			MutexLockGuard lock(mutex_);
			if (!availdIds_.empty())//不为空
			{
				id = availdIds_.front();//返回一个id使用
				availdIds_.pop();//出队列
				clientConns_[id] = conn;//重复的直接覆盖
			}
		}

		if (id <= 0)//未分配资源/或者资源已被使用完
		{
			conn->shutdown();//直接拒绝连接
		}
		else
		{
			conn->setContext(id);//将映射的key值保存在TcpConnection中
			char buf[256];
			snprintf(buf, sizeof(buf), "CONN %d FROM %s IS UP\r\n", id,
				conn->peerAddress().toIpPort().c_str());
			//通过内部连接（0）和文本协议通知后台
			sendBackendString(0, buf);//通知backend,此时id==0
		}
	}
	else//断开连接
	{
		if (!(conn->getContext().empty()))//如果conn里any有内容
		{
			//先取出占用的id
			int id = boost::any_cast<int>(conn->getContext());
			assert(id > 0 && id <= kMaxConns);
			char buf[256];
			snprintf(buf, sizeof(buf), "CONN %d FROM %s IS DOWN\r\n", id,
				conn->peerAddress().toIpPort().c_str());
			//通过内部连接（0）和文本协议通知后台
			sendBackendString(0, buf);//通知backend,此时id==0

			MutexLockGuard lock(mutex_);
			if (backendConn_)//如果后台已经连接
			{
				availdIds_.push(id);//回收id,循环使用
				LOG_DEBUG;
				clientConns_.erase(id);//删除断开的连接
				LOG_DEBUG;
			}
			else//未连接
			{
				assert(availdIds_.empty());
				assert(clientConns_.empty());
			}
		}
	}
	LOG_DEBUG;
}


//void multiplexer::MultiPlexServer::onClientMessage(const TcpConnectionPtr& conn,
//												   Buffer* buf,
//												   Timestamp receiveTime)
//{
//	size_t len = buf->readableBytes();//获取收到的数据包长度
//	transferred_.addAndGet(len);//temp +=len;
//	receivedMessages_.incrementAndGet();//++numb;
//	if (!(conn->getContext().empty()))//已建立连接的
//	{
//		int id = boost::any_cast<int>(conn->getContext());
//		//sendBackendString(id， buf);//需要将buf可读指针偏移
//		// assert(buf->readableBytes() == 0);
//	}
//	else
//	{
//		LOG_WARN << "Error Connection:["<<conn->localAddress().toIpPort()<<"]";
//		//buf->retrieveAll();//discard this message
//		buf->retrieve(len);
//		
//	}
//}


//void multiplexer::MultiPlexServer::onBackendMessage(const TcpConnectionPtr& conn,
//													Buffer* buf,
//													Timestamp receiveTime)
//{
//	size_t len = buf->readableBytes();//获取收到的数据包长度
//	transferred_.addAndGet(len);//temp +=len;
//	receivedMessages_.incrementAndGet();//++numb;
//	//sendToClient(buf);//目标放在数据包中
//}

void multiplexer::MultiPlexServer::sendToClient(int id, const char* payload, int payloadLen)
{
	TcpConnectionPtr clientConn;
	{
		MutexLockGuard lock(mutex_);
		std::map<int, TcpConnectionPtr>::iterator it = clientConns_.find(id);
		if (it != clientConns_.end())
		{
			clientConn = it->second;//仍然是增加引用，防止在使用的过程中被释放
		}
	}


	if (clientConn)//存在
	{
		//注意，这里是继续使用inputbuf还是用零时变量？
		//有可能inputbuf大量阻塞，而导致数据被覆盖。不过，这是小概率事件。
		const std::string message(payload, payloadLen);
		clientConn->send(message);
	}
	else
	{
		if (id == 0)//inner cmd
		{
			parseBackendInnerCommand(payload);
		}
		else
		{
			LOG_DEBUG << "\r\nrecv unkown id is: " << id;
		}
	}
}


void multiplexer::MultiPlexServer::parseBackendInnerCommand(const char* cmd)
{
	const std::string str = cmd;
	static const std::string kListen = "LISTEN ";
	static const std::string kLimit = "SET LIMIT ";
	static const std::string kDisconnect = "DISCONNECT ";
	bool isListen = str.find(kListen) != std::string::npos;
	bool isLimit = str.find(kLimit) != std::string::npos;
	bool isDisconnect = str.find(kDisconnect) != std::string::npos;
	if (isLimit)
	{
		auto maxConnsValue = atoi(&str[kLimit.size()]);
		LOG_DEBUG << "Set limit: " << maxConnsValue;
	}
	else if (isListen)
	{
		auto actionNumb = atoi(&str[kListen.size()]);
		LOG_DEBUG << "Set listen action: " << actionNumb;
	}
	else if (isDisconnect)
	{
		auto connId = atoi(&str[kDisconnect.size()]);
		assert(clientConns_.find(connId) != clientConns_.end());
		//if (clientConns_.find(connId) != clientConns_.end())
		{
			clientConns_[connId]->shutdown();//断开为外部连接。
		}
	}
	else
	{
		LOG_DEBUG << "recv err cmd: " << cmd;

	}


}

void multiplexer::MultiPlexServer::sendBackendString(int id, const string& msg)
{
	TcpConnectionPtr backendConn;
	{
		MutexLockGuard lock(mutex_);
		backendConn = backendConn_;//增加引用，防止在发送数据的过程中，对象被摧毁
	}

	if (backendConn)
	{
		assert(msg.size() <= multiplexer::kMaxPacketLen);
		Buffer packet;
		packet.append(msg);
		codec_.sendPacket(backendConn, id, &packet);
	}


}


void multiplexer::MultiPlexServer::sendBackenBuf(int id, Buffer* buf)
{
	TcpConnectionPtr backendConn;
	{
		MutexLockGuard lock(mutex_);
		backendConn = backendConn_;//增加引用，防止在发送数据的过程中，对象被摧毁
	}

	if (backendConn)
	{
		while (buf->readableBytes() > kMaxPacketLen)//若是多包，则需要分包发送
		{
			Buffer packet;
			packet.append(buf->peek(), kMaxPacketLen);
			buf->retrieve(kMaxPacketLen);//偏移索引
			codec_.sendPacket(backendConn, id, &packet);
		}
		if (buf->readableBytes() > 0)//仍然有剩余的，再发送一次
		{
			codec_.sendPacket(backendConn, id, buf);//可以直接使用偏移后的buf.
			//注意此处不用手动调用buf->retrieve，
			//因为TcpConnection::send里会调用buf->retrieveAll();
		}
	}



}