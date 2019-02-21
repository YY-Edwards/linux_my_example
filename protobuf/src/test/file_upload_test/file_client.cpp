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
		, dispatcher_(std::bind(&FileUploadClient::onUnknowMessage, this, _1, _2, _3))//注册一个无法识别的默认的回调
		//还可以注册一个解析出错的用户回调
		, codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3), NULL)
	{

		//注册收到确切protobuf类型的消息回调
		dispatcher_.registerMessageCallback<edwards::Answer>(
			std::bind(&FileUploadClient::onAnswer, this, _1, _2, _3));
		dispatcher_.registerMessageCallback<edwards::Empty>(
			std::bind(&FileUploadClient::onEmpty, this, _1, _2, _3));

		client_.setConnectionCallback(
			std::bind(&FileUploadClient::onConnection, this, _1));
		client_.setMessageCallback(
			std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));

	}
	~FileUploadClient() = default;

	void connect()
	{
		client_.connect();
		//client_.enableRetry();//开启断开重连功能
	}

	bool isConnected() const//函数里内容不允许改变
	{
		//指针有效且已连接，则返回true
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
				if (!avalidFileIds_.empty())//有可用id
				{
					id = avalidFileIds_.front();
					avalidFileIds_.pop();
					filesmap_[id] = theFile;//增加引用
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

				//if (messageToSend)
				//	codec_.send(conn, *messageToSend);
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
			if (found!=std::string::npos)
			{
				LOG_DEBUG << "file path: " << path.substr(0, found);
				LOG_DEBUG << "file name: " << path.substr(found+1);
				file->name = path.substr(found+1);
			}
			struct stat st;
			int ret = stat(path.c_str(), &st);
			assert(ret == 0);
			file->size = static_cast<int>(st.st_size);
			
			
			//定制删除器
			FilePtr newCtx(pFile, fclose);//当离开作用域是，会自动调用fclose
			file->ctx = newCtx;//增加引用
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
	void onAnswer(const muduo::net::TcpConnectionPtr& conn,
		const AnswerPtr& message,
		muduo::Timestamp t)
	{
		LOG_DEBUG << "onAnswer: \n" << message->GetTypeName()
			<< message->DebugString();

		message->PrintDebugString();

	}

	void onEmpty(const muduo::net::TcpConnectionPtr& conn,
		const EmptyPtr& message,
		muduo::Timestamp t)
	{

		LOG_DEBUG << "onEmpty: \n" << message->GetTypeName()
			<< message->DebugString();

		message->PrintDebugString();
	}

	void onUnknowMessage(const TcpConnectionPtr& conn,
		const MessagePtr& message,
		muduo::Timestamp)
	{
		LOG_DEBUG << "onUnknowMessage: \n" << message->GetTypeName();
	}


	void onConnection(const TcpConnectionPtr& conn)
	{
		LOG_INFO << conn->localAddress().toIpPort() << " -> "
			<< conn->peerAddress().toIpPort() << " is "
			<< (conn->connected() ? "UP" : "DOWN");

		if (conn->connected())
		{
			conn_ = conn;
			{
				MutexLockGuard lock(mutex_);
				assert(avalidFileIds_.empty());
				for (int i = 1; i <= kMaxFileId; i++)//分配文件上传时用到的编号
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
