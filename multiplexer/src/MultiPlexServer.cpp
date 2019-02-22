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
	//Ĭ�Ͽ���������������
	backend_.enableRetry();//�����Ͽ��Զ���������


	loop_->runEvery(10.0, boost::bind(&DescriptorCodec::printStatistics, &codec_));

}


void multiplexer::MultiPlexServer::start()
{
	//���ø���ioloop��ǰ�ûص�
	//server_.setThreadInitCallback(boost::bind(&MultiPlexServer::threadInitFunc, this, _1));
	//������Ե���Ϊ��backend�����Ƽ����˿ڵĿ�����رգ��Լ��ⲿ���ӵ�����
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
		for (int i = 1; i <= kMaxConns; i++)//����Ԥ����ID
		{
			availdIds_.push(i);
		}
	}
	else//�Ͽ�
	{
		MutexLockGuard lock(mutex_);
		backendConn_.reset();
		BOOST_FOREACH(auto it, clientConns_)//ȡ���������Ӷ���
		{	
			//ò�ƶ�ָ�����͵ı�׼����󲢲���Ҫ��ô��.
			connsToDestroy.emplace_back(std::move(it.second));//���Կ����᲻������������������ü���Ӧ�ü�1��
			//connsToDestroy.push_back(it.second);
			//emplace_back����Դ�����ֵ����it.second����ֵ
			//std::move(it.second)���Խ���ֵת��Ϊ��ֵ������ֵ������ֵ�ϡ�
			//(it.second)->shutdown();//stop write������ֱ�Ӳ���Ӧ�ò������������ڵ�Ӱ�죬��Ϊȫ��������
			//������Ϊ��Ҫ��������shutdown����Ч�ʲ��Ǻܸ�
		}
		clientConns_.clear();
		while (!availdIds_.empty())
		{
			availdIds_.pop();
		}
	}

	BOOST_FOREACH(auto it, connsToDestroy)
	{
		it->shutdown();//������readfd == 0;Ȼ��ص�onClientConnection(disconnected)=>erase��
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
			if (!availdIds_.empty())//��Ϊ��
			{
				id = availdIds_.front();//����һ��idʹ��
				availdIds_.pop();//������
				clientConns_[id] = conn;//�ظ���ֱ�Ӹ���
			}
		}

		if (id <= 0)//δ������Դ/������Դ�ѱ�ʹ����
		{
			conn->shutdown();//ֱ�Ӿܾ�����
		}
		else
		{
			conn->setContext(id);//��ӳ���keyֵ������TcpConnection��
			char buf[256];
			snprintf(buf, sizeof(buf), "CONN %d FROM %s IS UP\r\n", id,
				conn->peerAddress().toIpPort().c_str());
			//ͨ���ڲ����ӣ�0�����ı�Э��֪ͨ��̨
			sendBackendString(0, buf);//֪ͨbackend,��ʱid==0
		}
	}
	else//�Ͽ�����
	{
		if (!(conn->getContext().empty()))//���conn��any������
		{
			//��ȡ��ռ�õ�id
			int id = boost::any_cast<int>(conn->getContext());
			assert(id > 0 && id <= kMaxConns);
			char buf[256];
			snprintf(buf, sizeof(buf), "CONN %d FROM %s IS DOWN\r\n", id,
				conn->peerAddress().toIpPort().c_str());
			//ͨ���ڲ����ӣ�0�����ı�Э��֪ͨ��̨
			sendBackendString(0, buf);//֪ͨbackend,��ʱid==0

			MutexLockGuard lock(mutex_);
			if (backendConn_)//�����̨�Ѿ�����
			{
				availdIds_.push(id);//����id,ѭ��ʹ��
				LOG_DEBUG;
				clientConns_.erase(id);//ɾ���Ͽ�������
				LOG_DEBUG;
			}
			else//δ����
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
//	size_t len = buf->readableBytes();//��ȡ�յ������ݰ�����
//	transferred_.addAndGet(len);//temp +=len;
//	receivedMessages_.incrementAndGet();//++numb;
//	if (!(conn->getContext().empty()))//�ѽ������ӵ�
//	{
//		int id = boost::any_cast<int>(conn->getContext());
//		//sendBackendString(id�� buf);//��Ҫ��buf�ɶ�ָ��ƫ��
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
//	size_t len = buf->readableBytes();//��ȡ�յ������ݰ�����
//	transferred_.addAndGet(len);//temp +=len;
//	receivedMessages_.incrementAndGet();//++numb;
//	//sendToClient(buf);//Ŀ��������ݰ���
//}

void multiplexer::MultiPlexServer::sendToClient(int id, const char* payload, int payloadLen)
{
	TcpConnectionPtr clientConn;
	{
		MutexLockGuard lock(mutex_);
		std::map<int, TcpConnectionPtr>::iterator it = clientConns_.find(id);
		if (it != clientConns_.end())
		{
			clientConn = it->second;//��Ȼ���������ã���ֹ��ʹ�õĹ����б��ͷ�
		}
	}


	if (clientConn)//����
	{
		//ע�⣬�����Ǽ���ʹ��inputbuf��������ʱ������
		//�п���inputbuf�������������������ݱ����ǡ�����������С�����¼���
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
			clientConns_[connId]->shutdown();//�Ͽ�Ϊ�ⲿ���ӡ�
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
		backendConn = backendConn_;//�������ã���ֹ�ڷ������ݵĹ����У����󱻴ݻ�
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
		backendConn = backendConn_;//�������ã���ֹ�ڷ������ݵĹ����У����󱻴ݻ�
	}

	if (backendConn)
	{
		while (buf->readableBytes() > kMaxPacketLen)//���Ƕ��������Ҫ�ְ�����
		{
			Buffer packet;
			packet.append(buf->peek(), kMaxPacketLen);
			buf->retrieve(kMaxPacketLen);//ƫ������
			codec_.sendPacket(backendConn, id, &packet);
		}
		if (buf->readableBytes() > 0)//��Ȼ��ʣ��ģ��ٷ���һ��
		{
			codec_.sendPacket(backendConn, id, buf);//����ֱ��ʹ��ƫ�ƺ��buf.
			//ע��˴������ֶ�����buf->retrieve��
			//��ΪTcpConnection::send������buf->retrieveAll();
		}
	}



}