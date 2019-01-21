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
	//支持一对一的情况下，单线程就足以应付
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
		if (serverConn_)//目前仅支持1:1连接，仍有其它连接，直接断开
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
			break;//等待一个完整包
		}
		else//存在至少一个完整包
		{
			uint16_t connId = static_cast<uint8_t>(buf->peek()[1]);
			connId |= (static_cast<uint8_t>(buf->peek()[2] << 8));
			if (connId == 0)//内部通道
			{
				//std::string cmd = (buf->peek() + kHeaderLen);
				//(5) from buffer:
				//Copies the first n characters from the array of characters pointed by s.
				std::string cmd(buf->peek() + kHeaderLen, len);
				doInnerCommand(cmd);//去建立或销毁各个通道
			}
			else//N个连接的消息
			{
				//首先确定connId的通道已经存在
				assert(socksConns_.find(connId) != socksConns_.end());
				//再确定通道的连接是否建立成功
				TcpConnectionPtr& socksConn = socksConns_[connId].connection;
				if (socksConn)//成功
				{
					//直接转发消息
					LOG_DEBUG << "forward message.";
					//先确认pending数据已清空
					//考虑，如果此时pending数据为清空该怎么办
					assert(socksConns_[connId].pending.readableBytes() == 0);
					socksConn->send(buf->peek() + kHeaderLen, len);
				}
				else//通道连接尚未建立成功
					//即可能正在建立时，就从外部发送数据过来，那么先缓存
				{
					//将数据拷贝到此通道绑定的buff中。
					//默认最大是1K，可自由伸缩
					LOG_DEBUG << "pending message.";
					socksConns_[connId].pending.append(buf->peek() + kHeaderLen, len);
				}
			  }
			  buf->retrieve(kHeaderLen +len);//数据处理完毕，偏移指针
		}
	}
}


void DemuxServer::doInnerCommand(const std::string& cmd)
{
	//根据文本协议格式解析
	static const std::string kConn = "CONN ";
	//Get character of string
	//使用string的下标操作获取字符，
	//再通过取地址符号获得起始地址
	//又根据协议格式[CONN id FROM ip:port IS UP\r\n]
	//id后的空格，因此通过如下方法取得id值。
	uint16_t connId = atoi(&cmd[kConn.size()]);
	bool isUp = cmd.find(" IS UP") != std::string::npos;
	if (isUp)
	{
		//首先确定不会重复
		assert(socksConns_.find(connId) == socksConns_.end());
		char connName[64];
		snprintf(connName, sizeof connName, "SocksClient: %d", connId);
		//创建一个Entry
		Entry entry;
		entry.connId = connId;
		entry.client.reset(new TcpClient(loop_, socksAddr_, connName));
		entry.client->setConnectionCallback(
			std::bind(&DemuxServer::onSocksConnection, this, connId, _1));
		entry.client->setMessageCallback(
			std::bind(&DemuxServer::onSocksMessage, this, connId, _1, _2, _3));
		socksConns_[connId] = entry;
		entry.client->connect();//发起连接

	}
	else//通知断开
	{
		//注意，这里的断开可能触发两次：
		//第一种，外部主动断开，前端通知后台，此时如果conn与代理还未建立成功，直接从map里删除；
		//如果已建立，那么断开代理，等待代理断开回调，接着第二种。
		//第二种，代理服务主动断开，重置conn，后台通知前端，前端根据文本协议携带的ID查找对应的外部连接，
		//然后对外部连接进行shutdown操作，此时前端会触发断开回调，提示外部连接已断开，
		//此时前端再用内部端口通知后台，而此时的conn已经被重置，后台据文本协议内ID号，对map进行删除操作。
		assert(socksConns_.find(connId) != socksConns_.end());
		TcpConnectionPtr& socksConn = socksConns_[connId].connection;
		if (socksConn)
		{
			//主动断开代理服务
			socksConn->shutdown();
		}
		else
		{
			//说明已被重置，那么从map中删除
			socksConns_.erase(connId);
		}
	}
}


void DemuxServer::onSocksConnection(const uint16_t connId, const TcpConnectionPtr& conn)
{

	assert(socksConns_.find(connId) != socksConns_.end());
	if (conn->connected())
	{
		//因本服务是单线程，可以保证共享数据（socksConns_）的完整性,
		//因此不需要互斥锁保护资源。
		socksConns_[connId].connection = conn;
		//使用引用，避免拷贝
		Buffer& pending = socksConns_[connId].pending;
		if (pending.readableBytes() > 0)
		{
			conn->send((&pending));
		}
	}
	else
	{
		if(serverConn_)//如果本服务与Multiplexer建立的连接还在
		{
			LOG_DEBUG << "SOCKS4A DISCONNECT: " << connId;
			//这里，用内部特殊连接通知前端强制断开某个外部连接
			char buf[64];
			int len = snprintf(buf, sizeof buf, "DISCONNECT %d\r\n", connId);
			Buffer buffer;
			buffer.append(buf, len);
			sendPacketToServer(0, &buffer);//通知断开信息
			if (socksConns_[connId].connection)//这里应该需要重置TcpConnectionPtr
			{
				socksConns_[connId].connection.reset();
			}
			//考虑此处的socksConn什么时候删除：
			//一旦代理服务端主动断开了连接，只是用消息通知了前端
			//注意测试此断开逻辑
		}
		else//前端都已经不存在了
		{
			socksConns_.erase(connId);//直接删除
		}

	}
}

void DemuxServer::onSocksMessage(const uint16_t connId,
								 const TcpConnectionPtr& conn,
								 Buffer* buf,
								 Timestamp receiveTime)
{
	assert(socksConns_.find(connId) != socksConns_.end());
	while (buf->readableBytes() > kMaxPacketLen)//若是多包，则需要分包发送
	{
		Buffer packet;
		packet.append(buf->peek(), kMaxPacketLen);
		buf->retrieve(kMaxPacketLen);//偏移索引
		sendPacketToServer(connId, &packet);
	}
	if (buf->readableBytes() > 0)//仍然有剩余的，再发送一次
	{
		sendPacketToServer(connId, buf);
		//codec_.sendPacket(backendConn, id, buf);//可以直接使用偏移后的buf.
		//注意此处不用手动调用buf->retrieve，
		//因为TcpConnection::send里会调用buf->retrieveAll();
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

	buf->prepend(header, kHeaderLen);//用buf预留部分填充头信息，并更新buf偏移索引（向前提kHeaderLen）
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