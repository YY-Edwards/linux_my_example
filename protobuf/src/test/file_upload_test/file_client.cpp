/**
* file upload client implementation,file_client.h
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180221
*/



#include "Codec.h"
#include "Dispatcher.h"
#include "file_upload_proto2.pb.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>
#include <muduo/base/Mutex.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>


#include <map>
#include <set>
#include <stdio.h>
#include <queue>

using namespace muduo;
using namespace muduo::net;

const int8_t	kMaxFileId = 5;
const int8_t	kMaxPackageNumb = 0x9000;

typedef std::shared_ptr<FILE> FilePtr;

struct FileInfo
{
	FilePtr			ctx;
	std::string		name;
	int				size;
	std::string		format;
	std::string		storagePath;
};

class FileUploadClient :noncopyable
{
public:

	typedef std::shared_ptr<FileInfo> FileInfoPtr;
	typedef std::shared_ptr<edwards::UploadStartRequest> UploadStartRequestPtr;
	typedef std::shared_ptr<edwards::UploadStartReponse> UploadStartReponsePtr;
	typedef std::shared_ptr<edwards::FileFrameTransferRequest> FileFrameTransferRequestPtr;
	typedef std::shared_ptr<edwards::FileFrameTransferResponse> FileFrameTransferResponsePtr;
	typedef std::shared_ptr<edwards::UploadEndRequest> UploadEndRequestPtr;
	typedef std::shared_ptr<edwards::UploadEndResponse> UploadEndResponsePtr;


	explicit FileUploadClient(EventLoop *loop,
		const InetAddress& serverAddr,
		const std::string& name)
		:loop_(loop)
		, pn_(0x1000)
		, client_(loop, serverAddr, name)
		, dispatcher_(std::bind(&FileUploadClient::onUnknowMessage, this, _1, _2, _3))//ע��һ���޷�ʶ���Ĭ�ϵĻص�
		//������ע��һ������������û��ص�
		, codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3), NULL)
	{

		//ע���յ�ȷ��protobuf���͵���Ϣ�ص�
		dispatcher_.registerMessageCallback<edwards::UploadStartReponse>(
			std::bind(&FileUploadClient::onUploadStartReponse, this, _1, _2, _3));
		dispatcher_.registerMessageCallback<edwards::FileFrameTransferResponse>(
			std::bind(&FileUploadClient::onFileFrameTransferResponse, this, _1, _2, _3));
		dispatcher_.registerMessageCallback<edwards::UploadEndResponse>(
			std::bind(&FileUploadClient::onUploadEndResponse, this, _1, _2, _3));
		
		client_.setConnectionCallback(
			std::bind(&FileUploadClient::onConnection, this, _1));
		client_.setMessageCallback(
			std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));

		//loop_->runEvery();//request->response timeout resend.

	}
	~FileUploadClient() = default;

	void connect()
	{
		client_.connect();
		//client_.enableRetry();//�����Ͽ���������
	}

	bool isConnected() const//���������ݲ�����ı�
	{
		//ע�⣬���ܿ��̵߳��ã���ôΪ���̰߳�ȫ����Ҫ����
		//shared_ptr��дʱ��Ҫ����
		//ָ����Ч�������ӣ��򷵻�true
		return conn_ && conn_->connected();
	}


	void disconnect()
	{
		client_.disconnect();
	}

	void uploadTheFile(const std::string& fileName)
	{

		int id = -1;
		FileInfoPtr theFile;
		theFile.reset(parseFileInfo(fileName));
		if (theFile)
		{
			{
				MutexLockGuard lock(mutex_);
				if (!avalidFileIds_.empty())//�п���id
				{
					id = avalidFileIds_.front();
					avalidFileIds_.pop();
					filesmap_[id] = theFile;//��������
				}
			}

			if (id <= 0)
			{
				LOG_WARN << "file ids are used up!";
			}
			else
			{
				edwards::UploadStartRequest startReq;
				startReq.set_package_numb(pn_);
				increasePackageNumn();
				startReq.set_file_id(id);
				startReq.set_file_name(theFile->name);
				startReq.set_file_size(theFile->size);
				startReq.set_file_fromat(theFile->format);

				codec_.send(conn_, startReq);

			}
		}

	}

private:

	FileInfo* parseFileInfo(const std::string& path)
	{
		FileInfo* file = new FileInfo;

		FILE * pFile = fopen(path.c_str(), "rb");
		if (pFile != NULL)
		{
			std::size_t found = path.find_last_of("/");
			assert(found != std::string::npos);	
			LOG_DEBUG << "file path: " << path.substr(0, found);
			LOG_DEBUG << "file name: " << path.substr(found+1);
			file->name = path.substr(found+1);

			struct stat st;
			int ret = stat(path.c_str(), &st);
			assert(ret == 0);
			file->size = static_cast<int>(st.st_size);
			
			
			//����ɾ����
			FilePtr newCtx(pFile, fclose);//���뿪�������ǣ����Զ�����fclose
			file->ctx = newCtx;//��������
			file->format = "NULL";
			file->storagePath = "NULL";
		}		
		return file;
	}
	void increasePackageNumn()
	{ 
		pn_++;
		if (pn_ > kMaxPackageNumb)
		{
			pn_ = 0x1000;
		}
	}
	void onUploadStartReponse(const muduo::net::TcpConnectionPtr& conn,
		const UploadStartReponsePtr& message,
		muduo::Timestamp t)
	{
		LOG_DEBUG << "onUploadStartReponse: " << message->GetTypeName()
			<< "\n"
			<< message->DebugString();



		edwards::FileFrameTransferRequest frameReq;
		frameReq.set_package_numb(pn_);
		increasePackageNumn();
		frameReq.set_file_id(id);
		

		codec_.send(conn_, frameReq);
		

	}

	void onFileFrameTransferResponse(const muduo::net::TcpConnectionPtr& conn,
		const FileFrameTransferResponsePtr& message,
		muduo::Timestamp t)
	{

		LOG_DEBUG << "onFileFrameTransferResponse: " << message->GetTypeName()
			<< "\n"
			<< message->DebugString();

	}

	void onUploadEndResponse(const muduo::net::TcpConnectionPtr& conn,
		const UploadEndResponsePtr& message,
		muduo::Timestamp t)
	{

		LOG_DEBUG << "onUploadEndResponse: " << message->GetTypeName()
			<< "\n"
			<< message->DebugString();

	}


	void onUnknowMessage(const TcpConnectionPtr& conn,
		const MessagePtr& message,
		muduo::Timestamp)
	{
		LOG_DEBUG << "onUnknowMessage: \n" << message->GetTypeName();
	}

	void onWriteComplete(const TcpConnectionPtr& conn)
	{
		LOG_DEBUG << "\r\n";
		conn_->startRead();
		//�������õ�ˮλ�ص�������Ϊ�գ�������Ϊ�յ�ʱ��Ƶ���ص�
		conn_->setWriteCompleteCallback(muduo::net::WriteCompleteCallback());
	}

	void onHighWaterMark(const TcpConnectionPtr& conn, size_t len)
	{
		LOG_DEBUG << " onHighWaterMark " << conn->name()
			<< " bytes " << len;

		if (conn_ == conn)
		{
			conn_->stopRead();
			conn_->setWriteCompleteCallback(
				std::bind(&FileUploadClient::onWriteComplete, this, _1));
		}
	}

	void onConnection(const TcpConnectionPtr& conn)
	{
		LOG_INFO << conn->localAddress().toIpPort() << " -> "
			<< conn->peerAddress().toIpPort() << " is "
			<< (conn->connected() ? "UP" : "DOWN");

		if (conn->connected())
		{
			conn_ = conn;
			//���ø�ˮλ
			conn_->setHighWaterMarkCallback(
				std::bind(&FileUploadClient::onHighWaterMark, this, _1, _2), 
				64*1024);
			{
				MutexLockGuard lock(mutex_);
				assert(avalidFileIds_.empty());
				for (int i = 1; i <= kMaxFileId; i++)//�����ļ��ϴ�ʱ�õ��ı��
				{
					avalidFileIds_.push(i);
				}
			}

		}
		else
		{
			conn_.reset();
			{
				MutexLockGuard lock(mutex_);		
				while (!avalidFileIds_.empty())
				{
					avalidFileIds_.pop();
				}
			}

			loop_->quit();
		}
	}



	EventLoop*			loop_;
	TcpConnectionPtr	conn_;
	TcpClient			client_;
	ProtobufDispatcher	dispatcher_;
	ProtobufCodec		codec_;
	int64_t				pn_;//0x1000~0x9000;

	MutexLock			mutex_;

	std::map<int, FileInfoPtr>	filesmap_;
	std::queue<int>				avalidFileIds_;
};
