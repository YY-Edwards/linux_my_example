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
		, dispatcher_(std::bind(&FileUploadClient::onUnknowMessage, this, _1, _2, _3))//ע��һ���޷�ʶ���Ĭ�ϵĻص�
		//������ע��һ������������û��ص�
		, codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3), NULL)
	{

		//ע���յ�ȷ��protobuf���͵���Ϣ�ص�
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
		client_.enableRetry();//�����Ͽ���������
	}

	bool isConnected() const//���������ݲ�����ı�
	{
		//ע�⣬���ܿ��̵߳��ã���ôΪ���̰߳�ȫ����Ҫ����
		//shared_ptr���̶߳�дʱ��Ҫ����
		//ָ����Ч�������ӣ��򷵻�true
		MutexLockGuard lock(pMutex_);
		return conn_ && conn_->connected();//������
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
				if (!avalidFileIds_.empty())//�п���id
				{
					id = avalidFileIds_.front();
					avalidFileIds_.pop();
					filesmap_[id] = theFile;//��������
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
			
			
			//����ɾ����
			FilePtr newCtx(pFile, onCloseFileDescriptor);//���뿪�������ǣ����Զ�����fclose
			file->ctx = newCtx;//��������
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
				if (it != filesmap_.end())//����
				{
					findFile = (it->second);//��������
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
		if (message->package_numb() % 700 == 0)//ÿ700�ε��ã����һ����־
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
			if (it != filesmap_.end())//����
			{
				findFile = (it->second);//��������
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
					if (it != filesmap_.end())//����
					{
						findFile = (it->second);//��������
					}
				}
				if (findFile)
				{
					LOG_DEBUG << "Resend UploadEnd request.";
					sendEndReqToBackend(recvId, findFile);
				}
			}
		}
		else//�ϴ�����������
		{
			//ȡ����ռ��id
			int Id = message->file_id();
			assert(Id > 0 && Id <= kMaxFileId);
			{
				MutexLockGuard lock(mutex_);
				avalidFileIds_.push(Id);//�����ѷ����id
				LOG_DEBUG;
				auto numb = filesmap_.erase(Id);//ɾ�����ϴ����ļ���Ϣ
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
		//�������õ�ˮλ�ص�������Ϊ�գ�������Ϊ�յ�ʱ��Ƶ���ص�
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
			conn_ = conn;//д����
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

			//ע����17sΪ�����������Ϣ
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
	//mutable���Ա�֤��const���εĳ�Ա�������޸�ĳ����Ӱ����״̬�ı���
	mutable MutexLock			mapMutex_;
	//MapPtr					filesmap_;//����������һ���ã���ô����ʱ�ݲ�ʹ��COW�ַ�
	FileContainer				filesmap_;//����ļ�����Ϣ��
	std::queue<int>				avalidFileIds_;//�������Ĵ����ļ�ID��
};



void memstat()
{
	//valgrindҲ��й¶��鹤�ߣ��Ƚ�ȫ��

	//ͳ�Ʊ����̾�����ڴ�ʹ���������ȷ���ֽ�

	//Arena 0://��һ��arena��ÿ���̷߳���һ��arena��������ֻ��һ���߳�
	//system bytes     =     135168		//���̴߳Ӳ���ϵͳ��õĶ�̬�ڴ棬������132KB
	//in use bytes     =       1152		//���߳���ʹ�õĶ�̬�ڴ棬1152�ֽ�
	//Total (incl. mmap):				//�ܵ�ʹ������������߳�ʹ�ö�̬�ڴ���ۼ�ֵ
	//system bytes     =     135168		//�����̴Ӳ���ϵͳ��õĶ�̬�ڴ棬������132KB
	//in use bytes     =       1152		//��������ʹ�õĶ�̬�ڴ棬1152�ֽ�
	//max mmap regions =          0		//��һ�������ڴ泬��128KB��32λ����ϵͳ����1MB��64λ����ϵͳ��ʱ��������mmap��������ͳ��ʹ��mmap����ĸ���
	//max mmap bytes   =          0		//mmap�����Ӧ�ڴ��С

	LOG_INFO << "mem check:\n";
	malloc_stats();//���Լ���ڴ�й¶
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
				g_loop->runEvery(17.0, memstat);//�����ѯ�ڴ����

				FileUploadClient client(g_loop, InetAddress(hostip, port), "UpLoadFileClient");
				client.connect();

				string line;
				//ע������Ĭ�ϻس���ֹͣ����, ��Ctrl + Z�����EOF�س������˳�ѭ��
				while (getline(std::cin, line))//�ֶ������ϴ��ļ�
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