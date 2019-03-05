/**
* File server implementation,FileServer.h
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180301
*/


#include "FileServer.h"

using namespace muduo;
using namespace muduo::net;

using namespace edwards;

FileServer::FileServer(EventLoop *loop,
					   const InetAddress& listenAddr,
					   int maxConnections,
					   const std::string& name)
					   : loop_(loop)
					   , kMaxConnections_(maxConnections)
					   , numConnected_(0)
					   , server_(loop, listenAddr, name)
					   , dispatcher_(std::bind(&FileServer::onUnknowMessage, this, _1, _2, _3))//ע��һ���޷�ʶ���Ĭ�ϵĻص�
					   //������ע��һ������������û��ص�
					   , codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3), NULL)
{

	//ע���յ�ȷ��protobuf���͵���Ϣ�ص�
	dispatcher_.registerMessageCallback<edwards::UploadStartRequest>(
		std::bind(&FileServer::onUploadStartRequest, this, _1, _2, _3));
	dispatcher_.registerMessageCallback<edwards::FileFrameTransferRequest>(
		std::bind(&FileServer::onFileFrameTransferRequest, this, _1, _2, _3));
	dispatcher_.registerMessageCallback<edwards::UploadEndRequest>(
		std::bind(&FileServer::onUploadEndRequest, this, _1, _2, _3));

	server_.setConnectionCallback(
		std::bind(&FileServer::onConnection, this, _1));
	server_.setMessageCallback(
		std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));

	pool_.setMaxQueueSize(5);
	pool_.start(5);//�趨�̳߳ش�С

}


void FileServer::start()
{
	server_.start();
}
void FileServer::setThreadNumb(int numb)
{
	//�������߳�ģʽ
	server_.setThreadNum(numb);
}

void FileServer::onConnection(const TcpConnectionPtr& conn)
{
	LOG_INFO << conn->localAddress().toIpPort() << " -> "
		<< conn->peerAddress().toIpPort() << " is "
		<< (conn->connected() ? "UP" : "DOWN");

	edwards::ClientFile* clientFilePtr = NULL;

	if (conn->connected())
	{
		++numConnected_;
		if (numConnected_ > kMaxConnections_)
		{
			conn->shutdown();
			conn->forceCloseWithDelay(3.0);  // > round trip of the whole Internet.
		}
		else
		{
			//����һ���ͻ��ļ����󣬲���TcpConnectionPtr��
			conn->setContext(edwards::ClientFile());
			//ȡ��ÿһ���ͻ��˰󶨵��ļ�����
			clientFilePtr = boost::any_cast<edwards::ClientFile>(conn->getMutableContext());
			assert(clientFilePtr);
			if (pool_.isPoolFree())
			{
				LOG_TRACE << "addTask.";
				pool_.addTask(std::bind(&ClientFile::writeFileFunc, clientFilePtr));
			}
		}

	}
	else//�Ͽ�����
	{
		--numConnected_;
		//ȡ��ÿһ���ͻ��˰󶨵��ļ�����
		 clientFilePtr = boost::any_cast<edwards::ClientFile>(conn->getMutableContext());
		 if (clientFilePtr)
		 {
			 clientFilePtr->exitDownloadAndClose();
		 }

	}
	LOG_INFO << "numConnected = " << numConnected_;
	LOG_DEBUG << "clientFilePtr: " << clientFilePtr;
}

void FileServer::onUploadStartRequest(const muduo::net::TcpConnectionPtr& conn,
									  const UploadStartRequestPtr& message,
									  muduo::Timestamp t)
{
	LOG_DEBUG << "onUploadStartRequest: " << message->GetTypeName()
				<< "\n"
				<< message->DebugString();

	//ȡ��ÿһ���ͻ��˰󶨵��ļ�����
	edwards::ClientFile* clientFilePtr =
		boost::any_cast<edwards::ClientFile>(conn->getMutableContext());
	assert(clientFilePtr);
	bool ret;
	std::string result = "success";
	std::string reason = "";


	ret = clientFilePtr->create(message->file_id(), message->file_name(), message->file_size());
	if (ret != true)
	{
		result = "failure";
		reason = "file info err.";		
	}

	sendStartResponse(message->package_numb(), message->file_id(), result, reason);
}

void FileServer::onFileFrameTransferRequest(const muduo::net::TcpConnectionPtr& conn,
											const FileFrameTransferRequestPtr& message,
									        muduo::Timestamp t)
{
	LOG_DEBUG << "onFileFrameTransferRequest: " << message->GetTypeName()
		<< "\n"
		<< message->DebugString();

	//ȡ��ÿһ���ͻ��˰󶨵��ļ�����
	edwards::ClientFile* clientFilePtr =
		boost::any_cast<edwards::ClientFile>(conn->getMutableContext());
	assert(clientFilePtr);
	bool ret;
	std::string result = "success";
	std::string reason = "";


	clientFilePtr->appendContent(message->file_id(), message->frame_datas().c_str(), message->frame_size());

	sendFrameResponse(message->package_numb(), message->file_id(), message->frame_size(), result, reason);
}



void FileServer::onUploadEndRequest(const muduo::net::TcpConnectionPtr& conn,
									const UploadEndRequestPtr& message,
									muduo::Timestamp t)
{
	LOG_DEBUG << "onUploadEndRequest: " << message->GetTypeName()
		<< "\n"
		<< message->DebugString();

	//ȡ��ÿһ���ͻ��˰󶨵��ļ�����
	edwards::ClientFile* clientFilePtr =
		boost::any_cast<edwards::ClientFile>(conn->getMutableContext());
	assert(clientFilePtr);
	bool ret;
	std::string result = "success";
	std::string reason = "";

	clientFilePtr->close(message->file_id());

	sendFrameResponse(message->package_numb(), message->file_id(), message->frame_size(), result, reason);
}




