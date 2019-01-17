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

	//Tunnel��Ȼ�ǽ�ֹ�����ģ����Ǽ̳�enable_shared_from_this����ô����thisָ��ʱ
	//������shared_from_this();
	client_.setConnectionCallback(
		std::bind(&Tunnel::onClientConnection, shared_from_this(), _1));
		//boost::bind(&Tunnel::onClientConnection, shared_from_this(),_1));
	client_.setMessageCallback(
		std::bind(&Tunnel::onClientMessage, shared_from_this(), _1, _2, _3));
		//boost::bind(&Tunnel::onClientMessage, shared_from_this(), _1, _2, _3));

	if (serverConn_)
	{
		//�˴��ӳ����������ڣ��ص���ʱ��Tunnel����ʼ�մ��ڡ�
		//����ֵ�������ݣ���ô�ص�ʱ����ָ�����ü�����1���ص�������1.
		//serverConn_->setHighWaterMarkCallback(
		//	boost::bind(&Tunnel::onHighWaterMarkWeak, shared_from_this(), _1, _2),
		//	1024 * 1024);//����ˮλֵ��1M

		//�������������ص���������weak_ptr�󶨵�boost::function��.�ڻص���ʱ����������
		//�ɹ���˵�������ڣ�ʧ����˵�����󱻴ݻ��ˡ�
		//serverConn_->setHighWaterMarkCallback(
		//	boost::bind(&Tunnel::onHighWaterMarkWeak, 
		//	shared_from_this(),//����һ��ռλ��
		//	boost::weak_ptr<Tunnel>(shared_from_this()), KServer, _1, _2),//����
		//	1024 * 1024);//����ˮλֵ��1M
		//����ɾ�̬��Ա��������ô�Ͳ�������һ��ռλ�������ұȽ�ֱ�ۣ�
		//�˴�serverConn_���������ڲ���֪��
		//Tunnel�����ڿ���С��serverConn_,�����serverConn_�ص�ʱ��Ҫ�ж�Tunnel�Ƿ񻹴���
		//���serverConn_�ﻹ��Ҫ����Tunnel,��ô���Բο�TcpConnection����ָ�뵽Channel��������
		serverConn_->setHighWaterMarkCallback(
			std::bind(&Tunnel::onHighWaterMarkWeak,
			std::weak_ptr<Tunnel>(shared_from_this()), KServer, _1, _2),//����
			//boost::bind(&Tunnel::onHighWaterMarkWeak, 
			//boost::weak_ptr<Tunnel>(shared_from_this()), KServer, _1, _2),//����
			1024 * 1024);//����ˮλֵ��1M

	}

}

void Tunnel::connect()
{
	client_.connect();

}
void Tunnel :: disconnect()
{
	client_.disconnect();
	//serverConn_.reset();//���ü�����1
	//���������������Tunnel���������ڣ���ôϵͳ���Զ���������ָ�롣
	//����serverConn_��������ü������������Ƿ��1
	
}

void Tunnel::teardown()
{
	client_.setConnectionCallback(muduo::net::defaultConnectionCallback);
	client_.setMessageCallback(muduo::net::defaultMessageCallback);
	if (serverConn_)
	{
		serverConn_->setContext(boost::any());//���ñ��������
		serverConn_->shutdown();//�Ͽ�serverConn_����������
		//server��Դ��ʱ���ͷ�
	}
	clientConn_.reset();//���ü�����1
	

}

void Tunnel::onClientConnection(const TcpConnectionPtr& conn)
{
	LOG_DEBUG << (conn->connected() ? "UP" : "DOWN");
	if (conn->connected())
	{
		clientConn_ = conn;
		conn->setTcpNoDelay(true);//�ر�С����Ĭ�㷨
		//���ӳɹ��󣬲������ø�ˮλ�ص�
		conn->setHighWaterMarkCallback(
			std::bind(&Tunnel::onHighWaterMarkWeak,
			std::weak_ptr<Tunnel>(shared_from_this()), KClient, _1, _2),//����
			//boost::bind(&Tunnel::onHighWaterMarkWeak,
			//boost::weak_ptr<Tunnel>(shared_from_this()), KClient, _1, _2),//����
			1024 * 1024);//����ˮλֵ��1M);
		//������ʱ����Ҫʹ��
		serverConn_->setContext(conn);//���������ӵ�ָ�뱣����serverConn_�У����peer��
		serverConn_->startRead();//�򿪶����ݹ���
		//�п����ⲿ��serverConn_���ӳɹ���ͷ��������ݣ����Ǵ�ʱ�ڲ����client��δ�������ӣ�������ǰ�ر���serverConn_�Ķ����ݹ���
		//�п��ܴ�ʱ�����������ڻ������С�
		if (serverConn_->inputBuffer()->readableBytes() > 0)//�պ������ݣ����Է��������¼��ĸ���
		{
			conn->send(serverConn_->inputBuffer());//ֱ��ת�����ڲ����Զ�ƫ��buffָ��
		}
	}
	else
	{   //��Զ��sever�����Ͽ�clientConn_ʱ����ô��Ӧ�������Ͽ�serverConn_
		teardown();
	}
}

void Tunnel::onClientMessage(const TcpConnectionPtr& conn,
							 Buffer* buf,
							 Timestamp receiveTime)
{
	LOG_DEBUG << conn->name() << "send :" << buf->readableBytes() << " bytes.";
	
	//�����client���յ�remote server���͵�����
	if (serverConn_)//serverConn_���������ӻ��ڵĻ�
	{
		//�������server�˷���client���յ�������
		serverConn_->send(buf);
	}
	else
	{
		buf->retrieveAll();
		abort();//�˳��������쳣
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

	if (which == KServer)//serverConn_�����Ѵﵽ��ˮλ
	{
		if (serverConn_->outputBuffer()->readableBytes() > 0)//�ٴ�ȷ��
		{
			clientConn_->stopRead();//�ر�clientConn_��ȡ����
			//���õ�ˮλ�ص�������Ϊ�գ����������¿���clientConn_������
			serverConn_->setWriteCompleteCallback(
				std::bind(&Tunnel::onWriteCompleteWeak,
				std::weak_ptr<Tunnel>(shared_from_this()), KServer, _1));
				//boost::bind(&Tunnel::onWriteCompleteWeak,
				//boost::weak_ptr<Tunnel>(shared_from_this()), KServer, _1));
		}

	}
	else
	{

		if (clientConn_->outputBuffer()->readableBytes() > 0)//�ٴ�ȷ��
		{
			serverConn_->stopRead();//�ر�serverConn_��ȡ����
			//���õ�ˮλ�ص�������Ϊ�գ����������¿���serverConn_������
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

	if (which == KServer)//serverConn_�����Ѵﵽ��ˮλ
	{
		clientConn_->startRead();//����clientConn_��ȡ����
		//�������õ�ˮλ�ص�������Ϊ�գ�������Ϊ�յ�ʱ��Ƶ���ص�
		serverConn_->setWriteCompleteCallback(muduo::net::WriteCompleteCallback());
	}
	else
	{

		serverConn_->startRead();//����serverConn_��ȡ����
		//�������õ�ˮλ�ص�������Ϊ�գ�������Ϊ�յ�ʱ��Ƶ���ص�
		clientConn_->setWriteCompleteCallback(muduo::net::WriteCompleteCallback());
	}
}
