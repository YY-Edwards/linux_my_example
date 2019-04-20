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
#include <muduo/net/EventLoopThread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <malloc.h>
#include <sys/resource.h>

#include <map>
#include <set>
#include <stdio.h>
#include <queue>

using namespace muduo;
using namespace muduo::net;

const int8_t	kMaxFileId = 5;
const int8_t	kHeartbeatCycle = 17;//s
const uint16_t	kMaxPackageNumb = 0x9000;
const int		kBufSize = 10 * 1024;//10k

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
	typedef std::map<int, FileInfoPtr> FileContainer;
	typedef std::shared_ptr<FileContainer> MapPtr;

	typedef std::shared_ptr<edwards::UploadStartRequest> UploadStartRequestPtr;
	typedef std::shared_ptr<edwards::UploadStartResponse> UploadStartResponsePtr;
	typedef std::shared_ptr<edwards::FileFrameTransferRequest> FileFrameTransferRequestPtr;
	typedef std::shared_ptr<edwards::FileFrameTransferResponse> FileFrameTransferResponsePtr;
	typedef std::shared_ptr<edwards::UploadEndRequest> UploadEndRequestPtr;
	typedef std::shared_ptr<edwards::UploadEndResponse> UploadEndResponsePtr;


	explicit FileUploadClient(EventLoop *loop,
		const InetAddress& serverAddr,
		const std::string& name)
		: loop_(loop)
		, pn_(0x1000)
		, client_(loop, serverAddr, name)
		, dispatcher_(std::bind(&FileUploadClient::onUnknowMessage, this, _1, _2, _3))//注册一个无法识别的默认的回调
		//还可以注册一个解析出错的用户回调
		, codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3), NULL)
	{

		//注册收到确切protobuf类型的消息回调
		dispatcher_.registerMessageCallback<edwards::UploadStartResponse>(
			std::bind(&FileUploadClient::onUploadStartResponse, this, _1, _2, _3));
		dispatcher_.registerMessageCallback<edwards::FileFrameTransferResponse>(
			std::bind(&FileUploadClient::onFileFrameTransferResponse, this, _1, _2, _3));
		dispatcher_.registerMessageCallback<edwards::UploadEndResponse>(
			std::bind(&FileUploadClient::onUploadEndResponse, this, _1, _2, _3));
		dispatcher_.registerMessageCallback<edwards::AppHeartbeatResponse>(
			std::bind(&FileUploadClient::onAppHeartbeatResponse, this, _1, _2, _3));
		
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
		client_.enableRetry();//开启断开重连功能
	}

	bool isConnected() const//函数里内容不允许改变
	{
		//注意，可能跨线程调用，那么为了线程安全，需要上锁
		//shared_ptr多线程读写时需要加锁
		//指针有效且已连接，则返回true
		MutexLockGuard lock(pMutex_);
		return conn_ && conn_->connected();//读操作
	}


	void disconnect()
	{
		client_.disconnect();
	}

	void stop()
	{
		client_.stop();
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
					//(*filesmap_)[id] = theFile;
				}
			}

			if (id <= 0)
			{
				LOG_WARN << "file ids are used up!";
			}
			else
			{
				sendStartReqToBackend(id, theFile);
			}
		}
		else
		{
			LOG_DEBUG << "open file failure!";
		}
	}

private:

	FileInfo* parseFileInfo(const std::string& path)
	{
		FileInfo* file = NULL; 

		FILE * pFile = fopen(path.c_str(), "rb");
		if (pFile != NULL)
		{
			std::size_t found = path.find_last_of("/");
			assert(found != std::string::npos);	
			LOG_DEBUG << "file path: " << path.substr(0, found);
			LOG_DEBUG << "file name: " << path.substr(found+1);
			file = new FileInfo;
			file->name = path.substr(found+1);

			struct stat st;
			int ret = stat(path.c_str(), &st);
			assert(ret == 0);
			file->size = static_cast<int>(st.st_size);
			
			
			//定制删除器
			FilePtr newCtx(pFile, onCloseFileDescriptor);//当离开作用域是，会自动调用fclose
			file->ctx = newCtx;//增加引用
			file->format = "NULL";
			file->storagePath = "NULL";
		}		
		return file;
	}
	static void onCloseFileDescriptor(FILE *fp)
	{
		LOG_DEBUG << "close fp. ";
		fclose(fp);
	}
	void sendAppHeartbeat()
	{
		edwards::AppHeartbeatRequest hbReq;
		hbReq.set_package_numb(pn_);
		increasePackageNumn();
		//ip:port:startTime:pid
		hbReq.set_identity_id(clientIdentityID_);
		LOG_DEBUG;
		{
			MutexLockGuard lock(pMutex_);
			codec_.send(conn_, hbReq);
		}
	
	}
	void sendStartReqToBackend(int assignedId, const FileInfoPtr& file)
	{
		edwards::UploadStartRequest startReq;
		startReq.set_package_numb(pn_);
		increasePackageNumn();
		startReq.set_file_id(assignedId);
		startReq.set_file_name(file->name);
		startReq.set_file_size(file->size);
		startReq.set_file_fromat(file->format);
		LOG_DEBUG;
		{
			MutexLockGuard lock(pMutex_);
			codec_.send(conn_, startReq);
		}
	}
	void sendFrameReqToBackend(int assignedId, const FileInfoPtr& file)
	{
		char buf[kBufSize] = {0};
		size_t nread = fread(buf, 1, sizeof buf, (file->ctx).get());
		if (nread > 0)
		{
			edwards::FileFrameTransferRequest frameReq;
			frameReq.set_package_numb(pn_);
			increasePackageNumn();
			frameReq.set_file_id(assignedId);
			frameReq.set_frame_size(nread);
			frameReq.set_frame_datas(buf, nread);
			{
				MutexLockGuard lock(pMutex_);
				codec_.send(conn_, frameReq);
			}

		}
		else
		{
			assert(nread == 0);
			LOG_DEBUG << "send UploadEnd request.";
			sendEndReqToBackend(assignedId, file);
		}
	}
	void sendEndReqToBackend(int assignedId, const FileInfoPtr& file)
	{
		edwards::UploadEndRequest EndReq;
		EndReq.set_package_numb(pn_);
		increasePackageNumn();
		EndReq.set_file_id(assignedId);
		EndReq.set_file_name(file->name);
		{
			MutexLockGuard lock(pMutex_);
			codec_.send(conn_, EndReq);
		}
	}
	void increasePackageNumn()
	{ 
		pn_++;
		if (pn_ > kMaxPackageNumb)
		{
			pn_ = 0x1000;
		}
	}
	void onAppHeartbeatResponse(const muduo::net::TcpConnectionPtr& conn,
		const UploadStartResponsePtr& message,
		muduo::Timestamp t)
	{
		
		LOG_DEBUG << message->GetTypeName()
			<< "\n"
			<< message->DebugString();
		
	}
	void onUploadStartResponse(const muduo::net::TcpConnectionPtr& conn,
		const UploadStartResponsePtr& message,
		muduo::Timestamp t)
	{
		LOG_DEBUG << "onUploadStartReponse: " << message->GetTypeName()
			<< "\n"
			<< message->DebugString();

		int recvId = message->file_id();
		LOG_DEBUG << "recv file id: " << recvId;

		{
			FileInfoPtr findFile;
			{
				MutexLockGuard lock(mutex_);
				FileContainer::iterator it = filesmap_.find(recvId);
				if (it != filesmap_.end())//存在
				{
					findFile = (it->second);//增加引用
				}
			}
			if (findFile)
			{
				sendFrameReqToBackend(recvId, findFile);
			}
		}
	}

	void onFileFrameTransferResponse(const muduo::net::TcpConnectionPtr& conn,
		const FileFrameTransferResponsePtr& message,
		muduo::Timestamp t)
	{

		int recvId = message->file_id();
		if (message->package_numb() % 700 == 0)//每700次调用，输出一次日志
		{
			LOG_DEBUG << "onFileFrameTransferResponse: " << message->GetTypeName()
				<< "\n"
				<< message->DebugString();

			LOG_DEBUG << "recv file id: " << recvId;
		}

		FileInfoPtr findFile;
		{
			MutexLockGuard lock(mutex_);
			FileContainer::iterator it = filesmap_.find(recvId);
			if (it != filesmap_.end())//存在
			{
				findFile = (it->second);//增加引用
			}
		}
		if (findFile)
		{
			sendFrameReqToBackend(recvId, findFile);
		}
		
	}

	void onUploadEndResponse(const muduo::net::TcpConnectionPtr& conn,
		const UploadEndResponsePtr& message,
		muduo::Timestamp t)
	{

		LOG_DEBUG << "onUploadEndResponse: " << message->GetTypeName()
			<< "\n"
			<< message->DebugString();


		if (message->result().compare("success") != 0)
		{
			int recvId = message->file_id();
			LOG_DEBUG << "recv file id: " << recvId;

			{
				FileInfoPtr findFile;
				{
					MutexLockGuard lock(mutex_);
					FileContainer::iterator it = filesmap_.find(recvId);
					if (it != filesmap_.end())//存在
					{
						findFile = (it->second);//增加引用
					}
				}
				if (findFile)
				{
					LOG_DEBUG << "Resend UploadEnd request.";
					sendEndReqToBackend(recvId, findFile);
				}
			}
		}
		else//上传服务端已完成
		{
			//取出已占用id
			int Id = message->file_id();
			assert(Id > 0 && Id <= kMaxFileId);
			{
				MutexLockGuard lock(mutex_);
				avalidFileIds_.push(Id);//回收已分配的id
				LOG_DEBUG;
				auto numb = filesmap_.erase(Id);//删除已上传的文件信息
				LOG_DEBUG;
				assert(numb != 0);
			}
			LOG_DEBUG << "UploadEnd file_id:"<< Id <<" okay.";
		}

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
		conn->startRead();
		//重新设置低水位回调（发送为空）：以免为空的时候频繁回调
		conn->setWriteCompleteCallback(muduo::net::WriteCompleteCallback());
	}

	void onHighWaterMark(const TcpConnectionPtr& conn, size_t len)
	{
		LOG_DEBUG << " onHighWaterMark " << conn->name()
			<< " bytes " << len;

		conn->stopRead();
		conn->setWriteCompleteCallback(
				std::bind(&FileUploadClient::onWriteComplete, this, _1));
	}

	void onConnection(const TcpConnectionPtr& conn)
	{
		LOG_INFO << conn->localAddress().toIpPort() << " -> "
			<< conn->peerAddress().toIpPort() << " is "
			<< (conn->connected() ? "UP" : "DOWN");

		MutexLockGuard lock(pMutex_);
		if (conn->connected())
		{
			char buf[64] = "";
			sprintf(buf, "%s:%lld:%u",
				conn->localAddress().toIpPort().c_str(),
				Timestamp::now().microSecondsSinceEpoch(),
				CurrentThread::tid());
			clientIdentityID_ = buf;
			LOG_DEBUG << "identityKey: " << clientIdentityID_.c_str();
			conn_ = conn;//写操作
			//设置高水位
			conn_->setHighWaterMarkCallback(
				std::bind(&FileUploadClient::onHighWaterMark, this, _1, _2), 
				64*1024);
			{
				MutexLockGuard lock(mutex_);
				assert(avalidFileIds_.empty());
				for (int i = 1; i <= kMaxFileId; i++)//分配文件上传时用到的编号
				{
					avalidFileIds_.push(i);
				}
			}

			//注册以17s为间隔的心跳消息
			//sendAppHeartbeat();
			//loop_->runEvery(kHeartbeatCycle, std::bind(&FileUploadClient::sendAppHeartbeat, this));
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
				LOG_DEBUG;
				filesmap_.clear();
				LOG_DEBUG;
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
	std::string			clientIdentityID_;

	MutexLock			mutex_;

	mutable MutexLock			pMutex_;

	//FileContainer				filesmap_;
	//mutable可以保证在const修饰的成员函数中修改某个不影响类状态的变量
	mutable MutexLock			mapMutex_;
	//MapPtr					filesmap_;//有两个绑在一起用，那么互斥时暂不使用COW手法
	FileContainer				filesmap_;//多个文件的信息表
	std::queue<int>				avalidFileIds_;//存放允许的传输文件ID号
};



void memstat()
{
	//valgrind也是泄露检查工具，比较全面

	//统计本进程具体的内存使用情况，精确到字节

	//Arena 0://第一个arena（每个线程分配一个arena），这里只有一个线程
	//system bytes     =     135168		//本线程从操作系统获得的动态内存，这里是132KB
	//in use bytes     =       1152		//本线程在使用的动态内存，1152字节
	//Total (incl. mmap):				//总的使用情况，各个线程使用动态内存的累加值
	//system bytes     =     135168		//本进程从操作系统获得的动态内存，这里是132KB
	//in use bytes     =       1152		//本进程在使用的动态内存，1152字节
	//max mmap regions =          0		//当一次申请内存超过128KB（32位操作系统）或1MB（64位操作系统）时，会增加mmap区域，这里统计使用mmap区域的个数
	//max mmap bytes   =          0		//mmap区域对应内存大小

	LOG_INFO << "mem check:\n";
	malloc_stats();//用以检查内存泄露
}


EventLoop* g_loop = NULL;
string g_topic;

int main(int argc, char* argv[])
{

	muduo::Logger::setLogLevel(Logger::DEBUG);
	LOG_INFO << "pid = " << getpid();

	if (argc < 3)
	{
		printf("Usage: %s [ip:listen_port] [-] \n", argv[0]);
	}
	else
	{
		string hostport = argv[1];
		auto colon = hostport.find(':');
		if (colon != string::npos)
		{
			string hostip = hostport.substr(0, colon);
			const uint16_t port = static_cast<uint16_t>(atoi(hostport.c_str() + colon + 1));
			g_topic = argv[2];
			if (g_topic == "-")
			{
				{
					// set max virtual memory to 256MB.
					size_t kOneMB = 1024 * 1024;
					rlimit rl = { 256 * kOneMB, 256 * kOneMB };
					setrlimit(RLIMIT_AS, &rl);
				}

				const uint16_t listenPort = static_cast<uint16_t>(atoi(argv[1]));

				EventLoopThread loopThread;
				g_loop = loopThread.startLoop();
				g_loop->runEvery(17.0, memstat);//间隔查询内存分配

				FileUploadClient client(g_loop, InetAddress(hostip, port), "UpLoadFileClient");
				client.connect();

				string line;
				//注意这里默认回车符停止读入, 按Ctrl + Z或键入EOF回车即可退出循环
				while (getline(std::cin, line))//手动控制上传文件
				{
					if (client.isConnected())
					{
						client.uploadTheFile(line);
					}
				}

				client.disconnect();
				LOG_DEBUG;
				client.stop();
				CurrentThread::sleepUsec(5000 * 1000);//5s

				LOG_DEBUG;
			}
		}
		
	}

	printf("\r\n=>>exit file_client.cpp \r\n");
}