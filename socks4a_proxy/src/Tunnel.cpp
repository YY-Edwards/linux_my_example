#include"Tunnel.h"


Tunnel::Tunnel(EventLoop * loop,
			   const InetAddress& serverAddr,
			   const TcpConnectionPtr& serverConn)
			   :loop_(loop)
			   , client_(loop, serverAddr, serverConn->name())
			   , serverConn_(serverConn)
{

	LOG_DEBUG << "Tunnel:From " << serverConn->peerAddress().toIpPort()
		<< " <-> To" << serverAddr.toIpPort();
}

Tunnel::~Tunnel()
{
	LOG_DEBUG << "~Tunnel:";
}

void Tunnel::setup()
{

	//Tunnel虽然是禁止拷贝的，但是继承enable_shared_from_this，那么传递this指针时
	//建议用shared_from_this();
	client_.setConnectionCallback(
		std::bind(&Tunnel::onClientConnection, shared_from_this(), _1));
		//boost::bind(&Tunnel::onClientConnection, shared_from_this(),_1));
	client_.setMessageCallback(
		std::bind(&Tunnel::onClientMessage, shared_from_this(), _1, _2, _3));
		//boost::bind(&Tunnel::onClientMessage, shared_from_this(), _1, _2, _3));

	if (serverConn_)
	{
		//此处延长了生命周期，回调的时候Tunnel对象始终存在。
		//绑定是值拷贝传递，那么回调时智能指针引用计数加1，回调结束减1.
		//serverConn_->setHighWaterMarkCallback(
		//	boost::bind(&Tunnel::onHighWaterMarkWeak, shared_from_this(), _1, _2),
		//	1024 * 1024);//设置水位值：1M

		//这里再启用弱回调技术：将weak_ptr绑定到boost::function里.在回调的时候先提升，
		//成功则说明对象还在，失败则说明对象被摧毁了。
		//serverConn_->setHighWaterMarkCallback(
		//	boost::bind(&Tunnel::onHighWaterMarkWeak, 
		//	shared_from_this(),//牺牲一个占位符
		//	boost::weak_ptr<Tunnel>(shared_from_this()), KServer, _1, _2),//参数
		//	1024 * 1024);//设置水位值：1M
		//定义成静态成员函数，那么就不用牺牲一个占位符，而且比较直观？
		//此处serverConn_的生命周期不可知，
		//Tunnel生命期可能小于serverConn_,因此在serverConn_回调时需要判断Tunnel是否还存在
		//如果serverConn_里还需要保存Tunnel,那么可以参考TcpConnection传递指针到Channel的做法。
		serverConn_->setHighWaterMarkCallback(
			std::bind(&Tunnel::onHighWaterMarkWeak,
			std::weak_ptr<Tunnel>(shared_from_this()), KServer, _1, _2),//参数
			//boost::bind(&Tunnel::onHighWaterMarkWeak, 
			//boost::weak_ptr<Tunnel>(shared_from_this()), KServer, _1, _2),//参数
			1024 * 1024);//设置水位值：1M

	}

}

void Tunnel::connect()
{
	client_.connect();

}
void Tunnel :: disconnect()
{
	client_.disconnect();
	//serverConn_.reset();//引用计数减1
	//这里外层会结束整个Tunnel的声明周期，那么系统会自动回收智能指针。
	//所以serverConn_这里的引用计数有外层控制是否减1
	
}

void Tunnel::teardown()
{
	client_.setConnectionCallback(muduo::net::defaultConnectionCallback);
	client_.setMessageCallback(muduo::net::defaultMessageCallback);
	if (serverConn_)
	{
		serverConn_->setContext(boost::any());//重置保存的内容
		serverConn_->shutdown();//断开serverConn_建立的连接
		//server资源暂时不释放
	}
	clientConn_.reset();//引用计数减1
	

}

void Tunnel::onClientConnection(const TcpConnectionPtr& conn)
{
	LOG_DEBUG << (conn->connected() ? "UP" : "DOWN");
	if (conn->connected())
	{
		clientConn_ = conn;
		conn->setTcpNoDelay(true);//关闭小包静默算法
		//连接成功后，才能设置高水位回调
		conn->setHighWaterMarkCallback(
			std::bind(&Tunnel::onHighWaterMarkWeak,
			std::weak_ptr<Tunnel>(shared_from_this()), KClient, _1, _2),//参数
			//boost::bind(&Tunnel::onHighWaterMarkWeak,
			//boost::weak_ptr<Tunnel>(shared_from_this()), KClient, _1, _2),//参数
			1024 * 1024);//设置水位值：1M);
		//在外层的时候需要使用
		serverConn_->setContext(conn);//将建立连接的指针保存在serverConn_中：隧道peer绑定
		serverConn_->startRead();//打开读数据功能
		//有可能外部的serverConn_连接成功后就发送了数据，但是此时内部这个client还未建立连接，并且提前关闭了serverConn_的读数据功能
		//有可能此时的数据阻塞在缓冲器中。
		if (serverConn_->inputBuffer()->readableBytes() > 0)//刚好有数据：测试发生这种事件的概率
		{
			conn->send(serverConn_->inputBuffer());//直接转发：内部会自动偏移buff指针
		}
	}
	else
	{   //当远程sever主动断开clientConn_时，那么相应的主动断开serverConn_
		teardown();
	}
}

void Tunnel::onClientMessage(const TcpConnectionPtr& conn,
							 Buffer* buf,
							 Timestamp receiveTime)
{
	LOG_DEBUG << conn->name() << "send :" << buf->readableBytes() << " bytes.";
	
	//隧道的client端收到remote server发送的数据
	if (serverConn_)//serverConn_建立的连接还在的话
	{
		//用隧道是server端发送client端收到的数据
		serverConn_->send(buf);
	}
	else
	{
		buf->retrieveAll();
		abort();//退出并报告异常
	}
}

void Tunnel::onHighWaterMarkWeak(const std::weak_ptr<Tunnel>& wkTunnel,//const boost::weak_ptr<Tunnel>& wkTunnel,
										 ObjID which,
										 const TcpConnectionPtr& conn,
										 size_t bytesToSent)
{
	//boost::shared_ptr<Tunnel> tunnel = wkTunnel.lock();
	std::shared_ptr<Tunnel> tunnel = wkTunnel.lock();
	if (tunnel)
	{
		tunnel->onHighWaterMark(which, conn, bytesToSent);
	}
	
}

void ::Tunnel::onHighWaterMark(ObjID which,
							   const TcpConnectionPtr& conn,
							   size_t bytesToSent)
{

	LOG_DEBUG << (which == KServer ? "server" : "client")
		<< " onHighWaterMark " << conn->name()
		<< " bytes " << bytesToSent;

	if (which == KServer)//serverConn_发送已达到高水位
	{
		if (serverConn_->outputBuffer()->readableBytes() > 0)//再次确认
		{
			clientConn_->stopRead();//关闭clientConn_读取数据
			//设置低水位回调（发送为空）：用以重新开启clientConn_读数据
			serverConn_->setWriteCompleteCallback(
				std::bind(&Tunnel::onWriteCompleteWeak,
				std::weak_ptr<Tunnel>(shared_from_this()), KServer, _1));
				//boost::bind(&Tunnel::onWriteCompleteWeak,
				//boost::weak_ptr<Tunnel>(shared_from_this()), KServer, _1));
		}

	}
	else
	{

		if (clientConn_->outputBuffer()->readableBytes() > 0)//再次确认
		{
			serverConn_->stopRead();//关闭serverConn_读取数据
			//设置低水位回调（发送为空）：用以重新开启serverConn_读数据
			clientConn_->setWriteCompleteCallback(
				std::bind(&Tunnel::onWriteCompleteWeak,
				std::weak_ptr<Tunnel>(shared_from_this()), KClient, _1));
				//boost::bind(&Tunnel::onWriteCompleteWeak,
				//boost::weak_ptr<Tunnel>(shared_from_this()), KClient, _1));
		}
	}
}


void Tunnel::onWriteCompleteWeak(const std::weak_ptr<Tunnel>& wkTunnel,//const boost::weak_ptr<Tunnel>& wkTunnel,
								 ObjID which,
								 const TcpConnectionPtr& conn)
{
	//boost::shared_ptr<Tunnel> tunnel = wkTunnel.lock();
	std::shared_ptr<Tunnel> tunnel = wkTunnel.lock();
	if (tunnel)
	{
		tunnel->onWriteComplete(which, conn);
	}

}

void ::Tunnel::onWriteComplete(ObjID which, const TcpConnectionPtr& conn)
{

	LOG_DEBUG << (which == KServer ? "server" : "client")
		<< " onWriteComplete " << conn->name();

	if (which == KServer)//serverConn_发送已达到低水位
	{
		clientConn_->startRead();//开启clientConn_读取数据
		//重新设置低水位回调（发送为空）：以免为空的时候频繁回调
		serverConn_->setWriteCompleteCallback(muduo::net::WriteCompleteCallback());
	}
	else
	{

		serverConn_->startRead();//开启serverConn_读取数据
		//重新设置低水位回调（发送为空）：以免为空的时候频繁回调
		clientConn_->setWriteCompleteCallback(muduo::net::WriteCompleteCallback());
	}
}
