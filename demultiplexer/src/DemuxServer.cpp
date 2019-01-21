/**
* DeMultiPlex Server implementation,DemuxServer.cpp
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180118
*/


#include "DemuxServer.h"

const size_t kMaxPacketLen = 255;
const size_t kHeaderLen = 3;

DemuxServer::DemuxServer(EventLoop* loop,
						const InetAddress& listenAddr,
						const InetAddress& serverAddr)
						:loop_(loop)
						, socksAddr_(serverAddr)
						,server_(loop, listenAddr, "DemuxServer")
{
	
	server_.setConnectionCallback(
		std::bind(&DemuxServer::onServerConnection, this, _1));
	server_.setMessageCallback(
		std::bind(&DemuxServer::onServerMessage, this , _1, _2, _3));

}

DemuxServer::~DemuxServer()
{

	LOG_DEBUG << "~DemuxServer";
}

void DemuxServer::start()
{	
	//֧��һ��һ������£����߳̾�����Ӧ��
	server_.start();
}

void DemuxServer::onServerConnection(const TcpConnectionPtr& conn)
{
	LOG_TRACE << conn->localAddress().toIpPort()
		<< " -> "
		<< conn->peerAddress().toIpPort()
		<< " is "
		<< (conn->connected() ? "UP" : "DOWN");

	if (conn->connected())
	{
		if (serverConn_)//Ŀǰ��֧��1:1���ӣ������������ӣ�ֱ�ӶϿ�
		{
			LOG_DEBUG << "\r\nsorry, Demux only servers one client.";
			conn->shutdown();
		}
		else
		{
			serverConn_ = conn;
			//set max conns
			setFrontMaxConns(10);
			LOG_INFO << "onServerConnection set serverConn_";
		}
	}
	else
	{
		if (serverConn_ == conn)
		{
			serverConn_.reset();
			socksConns_.clear();

			LOG_INFO << "onServerConnection reset serverConn_";
		}
	}

}

void DemuxServer::onServerMessage(const TcpConnectionPtr& conn,
								  Buffer* buf,
								  Timestamp receiveTime)
{

	LOG_DEBUG << conn->name() << "send: "
		<< buf->readableBytes()
		<< " bytes.";

	while (buf->readableBytes() > kHeaderLen)
	{
		uint8_t len = static_cast<uint8_t>(buf->peek()[0]);
		if (buf->readableBytes() < (len + kHeaderLen))
		{
			LOG_DEBUG << "wait for a complete message.";
			break;//�ȴ�һ��������
		}
		else//��������һ��������
		{
			uint16_t connId = static_cast<uint8_t>(buf->peek()[1]);
			connId |= (static_cast<uint8_t>(buf->peek()[2] << 8));
			if (connId == 0)//�ڲ�ͨ��
			{
				//std::string cmd = (buf->peek() + kHeaderLen);
				//(5) from buffer:
				//Copies the first n characters from the array of characters pointed by s.
				std::string cmd(buf->peek() + kHeaderLen, len);
				doInnerCommand(cmd);//ȥ���������ٸ���ͨ��
			}
			else//N�����ӵ���Ϣ
			{
				//����ȷ��connId��ͨ���Ѿ�����
				assert(socksConns_.find(connId) != socksConns_.end());
				//��ȷ��ͨ���������Ƿ����ɹ�
				TcpConnectionPtr& socksConn = socksConns_[connId].connection;
				if (socksConn)//�ɹ�
				{
					//ֱ��ת����Ϣ
					LOG_DEBUG << "forward message.";
					//��ȷ��pending���������
					//���ǣ������ʱpending����Ϊ��ո���ô��
					assert(socksConns_[connId].pending.readableBytes() == 0);
					socksConn->send(buf->peek() + kHeaderLen, len);
				}
				else//ͨ��������δ�����ɹ�
					//���������ڽ���ʱ���ʹ��ⲿ�������ݹ�������ô�Ȼ���
				{
					//�����ݿ�������ͨ���󶨵�buff�С�
					//Ĭ�������1K������������
					LOG_DEBUG << "pending message.";
					socksConns_[connId].pending.append(buf->peek() + kHeaderLen, len);
				}
			  }
			  buf->retrieve(kHeaderLen +len);//���ݴ�����ϣ�ƫ��ָ��
		}
	}
}


void DemuxServer::doInnerCommand(const std::string& cmd)
{
	//�����ı�Э���ʽ����
	static const std::string kConn = "CONN ";
	//Get character of string
	//ʹ��string���±������ȡ�ַ���
	//��ͨ��ȡ��ַ���Ż����ʼ��ַ
	//�ָ���Э���ʽ[CONN id FROM ip:port IS UP\r\n]
	//id��Ŀո����ͨ�����·���ȡ��idֵ��
	uint16_t connId = atoi(&cmd[kConn.size()]);
	bool isUp = cmd.find(" IS UP") != std::string::npos;
	if (isUp)
	{
		//����ȷ�������ظ�
		assert(socksConns_.find(connId) == socksConns_.end());
		char connName[64];
		snprintf(connName, sizeof connName, "SocksClient: %d", connId);
		//����һ��Entry
		Entry entry;
		entry.connId = connId;
		entry.client.reset(new TcpClient(loop_, socksAddr_, connName));
		entry.client->setConnectionCallback(
			std::bind(&DemuxServer::onSocksConnection, this, connId, _1));
		entry.client->setMessageCallback(
			std::bind(&DemuxServer::onSocksMessage, this, connId, _1, _2, _3));
		socksConns_[connId] = entry;
		entry.client->connect();//��������

	}
	else//֪ͨ�Ͽ�
	{
		//ע�⣬����ĶϿ����ܴ������Σ�
		//��һ�֣��ⲿ�����Ͽ���ǰ��֪ͨ��̨����ʱ���conn�����δ�����ɹ���ֱ�Ӵ�map��ɾ����
		//����ѽ�������ô�Ͽ������ȴ�����Ͽ��ص������ŵڶ��֡�
		//�ڶ��֣�������������Ͽ�������conn����̨֪ͨǰ�ˣ�ǰ�˸����ı�Э��Я����ID���Ҷ�Ӧ���ⲿ���ӣ�
		//Ȼ����ⲿ���ӽ���shutdown��������ʱǰ�˻ᴥ���Ͽ��ص�����ʾ�ⲿ�����ѶϿ���
		//��ʱǰ�������ڲ��˿�֪ͨ��̨������ʱ��conn�Ѿ������ã���̨���ı�Э����ID�ţ���map����ɾ��������
		assert(socksConns_.find(connId) != socksConns_.end());
		TcpConnectionPtr& socksConn = socksConns_[connId].connection;
		if (socksConn)
		{
			//�����Ͽ��������
			socksConn->shutdown();
		}
		else
		{
			//˵���ѱ����ã���ô��map��ɾ��
			socksConns_.erase(connId);
		}
	}
}


void DemuxServer::onSocksConnection(const uint16_t connId, const TcpConnectionPtr& conn)
{

	assert(socksConns_.find(connId) != socksConns_.end());
	if (conn->connected())
	{
		//�򱾷����ǵ��̣߳����Ա�֤�������ݣ�socksConns_����������,
		//��˲���Ҫ������������Դ��
		socksConns_[connId].connection = conn;
		//ʹ�����ã����⿽��
		Buffer& pending = socksConns_[connId].pending;
		if (pending.readableBytes() > 0)
		{
			conn->send((&pending));
		}
	}
	else
	{
		if(serverConn_)//�����������Multiplexer���������ӻ���
		{
			LOG_DEBUG << "SOCKS4A DISCONNECT: " << connId;
			//������ڲ���������֪ͨǰ��ǿ�ƶϿ�ĳ���ⲿ����
			char buf[64];
			int len = snprintf(buf, sizeof buf, "DISCONNECT %d\r\n", connId);
			Buffer buffer;
			buffer.append(buf, len);
			sendPacketToServer(0, &buffer);//֪ͨ�Ͽ���Ϣ
			if (socksConns_[connId].connection)//����Ӧ����Ҫ����TcpConnectionPtr
			{
				socksConns_[connId].connection.reset();
			}
			//���Ǵ˴���socksConnʲôʱ��ɾ����
			//һ���������������Ͽ������ӣ�ֻ������Ϣ֪ͨ��ǰ��
			//ע����Դ˶Ͽ��߼�
		}
		else//ǰ�˶��Ѿ���������
		{
			socksConns_.erase(connId);//ֱ��ɾ��
		}

	}
}

void DemuxServer::onSocksMessage(const uint16_t connId,
								 const TcpConnectionPtr& conn,
								 Buffer* buf,
								 Timestamp receiveTime)
{
	assert(socksConns_.find(connId) != socksConns_.end());
	while (buf->readableBytes() > kMaxPacketLen)//���Ƕ��������Ҫ�ְ�����
	{
		Buffer packet;
		packet.append(buf->peek(), kMaxPacketLen);
		buf->retrieve(kMaxPacketLen);//ƫ������
		sendPacketToServer(connId, &packet);
	}
	if (buf->readableBytes() > 0)//��Ȼ��ʣ��ģ��ٷ���һ��
	{
		sendPacketToServer(connId, buf);
		//codec_.sendPacket(backendConn, id, buf);//����ֱ��ʹ��ƫ�ƺ��buf.
		//ע��˴������ֶ�����buf->retrieve��
		//��ΪTcpConnection::send������buf->retrieveAll();
	}
	
}

void DemuxServer::sendPacketToServer(uint16_t connId, Buffer* buf)
{

	size_t len = buf->readableBytes();
	LOG_DEBUG << len;
	assert(len <= kMaxPacketLen);
	uint8_t header[kHeaderLen] = {
		static_cast<uint8_t>(len),
		static_cast<uint8_t>(connId & 0xFF),
		static_cast<uint8_t>((connId & 0xFF00) >> 8),
	};

	buf->prepend(header, kHeaderLen);//��bufԤ���������ͷ��Ϣ��������bufƫ����������ǰ��kHeaderLen��
	if (serverConn_)
	{
		serverConn_->send(buf);
	}
}

void DemuxServer::setFrontMaxConns(uint16_t numb)
{
	char buf[64];
	int len = snprintf(buf, sizeof buf, "SET LIMIT %d\r\n", numb);
	Buffer buffer;
	buffer.append(buf, len);
	sendPacketToServer(0, &buffer);

}

void DemuxServer::setFrontListenAction(uint16_t action)
{
	char buf[64];
	int len = snprintf(buf, sizeof buf, "LISTEN %d\r\n", action);//0:close; 1:open
	Buffer buffer;
	buffer.append(buf, len);
	sendPacketToServer(0, &buffer);

}