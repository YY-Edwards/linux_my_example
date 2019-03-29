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

#include <fcntl.h>
#include <sys/stat.h> 

using namespace muduo;
using namespace muduo::net;

using namespace edwards;

const std::string FirstPath = "/home/edwards/app/protobuf/connUploadFile/";

ClientFile::ClientFile(const std::string& clientName)
			:quit_(false)
			, connName_(clientName)
			, running_(false)
			, mutex_()
			, notRun_(mutex_)

{
	int status;
	storagePath_ = FirstPath + connName_;//每一个客户连接创建一个路劲

	if (access(FirstPath.c_str(), F_OK) != 0)//第一级目录
	{
		status = mkdir(FirstPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		if (status < 0)
		{
			LOG_WARN << "mkdir FirstPath err: " << strerror(errno);
		}
	}

	if ((access(storagePath_.c_str(), F_OK) != 0))//二级目录
	{
		status = mkdir(storagePath_.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		if (status < 0)
		{
			LOG_WARN << "mkdir storagePath err: " << strerror(errno);
		}
	}

	LOG_DEBUG;
}

ClientFile::~ClientFile()
{
	if (running_)
	{
		exitDownloadAndClose();
		//LOG_DEBUG;
		////3s后再退出对象。如果3s后对象已销毁，而线程池里的注册任务还没退出，
		////则可能发生未定义行为。
		//notRun_.waitForSeconds(3);
	}
	LOG_DEBUG;
}

bool ClientFile::create(int file_id, std::string fileName, uint64_t file_size)
{
	bool ret = false;
	FILE * pFile =NULL;
	std::string filePath = storagePath_ + "/" + fileName;
	pFile = fopen(filePath.c_str(), "wb");//打开或创建一个只写文件
	if (pFile != NULL)
	{
		//每一个文件设定一个输出缓冲区
		//所有写入到pFile的输出都应该使用buffer_作为输出缓冲区，
		//直到buffer_缓冲区被填满或者程序员直接调用fflush（译注：对于由写操作打开的文件，调用fflush将导致输出缓冲区的内容被实际地写入该文件），
		//buffer_缓冲区中的内容才实际写入到pFile

		LOG_DEBUG << "new pFile: " << pFile;

		//每一个文件缓冲区大小都按其文件的总大小的一定比列(20)设定。
		std::shared_ptr<char> buffPtr;
		if (file_size > kStreamBuffReduceRatio)
		{
			uint64_t mallocSize = file_size / kStreamBuffReduceRatio;
			std::shared_ptr<char> arrayPtr(new char[mallocSize], std::default_delete<char[]>());
			::setbuffer(pFile, arrayPtr.get(), mallocSize);
			buffPtr = arrayPtr;//增加引用
		}

		FileInfoPtr newFileInfoPtr(new ClientUploadFileInfo);//新建，不需要定时删除器，全部可以自动释放
		FilePtr p(pFile, onCloseFileDescriptor);//用已存在的构造一个新的

		newFileInfoPtr->ctx		= p;
		newFileInfoPtr->name	= fileName;
		newFileInfoPtr->size	= file_size;
		newFileInfoPtr->state	= kWaitToWrite;
		newFileInfoPtr->lenIndex = 0;
		newFileInfoPtr->storagePath = storagePath_;
		newFileInfoPtr->streamBuffPtr = buffPtr;

		{
			MutexLockGuard lock(mutex_);
			fileList_[file_id] = newFileInfoPtr;//相同则覆盖
		}
		ret = true;
	}

	return ret;
}

bool ClientFile::isFileExisted(int file_id)
{
	MutexLockGuard lock(mutex_);
	return fileList_.find(file_id) != fileList_.end() ? true : false;
}

bool ClientFile::appendContent(int file_id, const char* data, int dataLen)
{
	bool ret = false;
	if (!isFileExisted(file_id))return false;

	if (dataLen <= kPayloadSize)
	{
		DataUnit dataUnit;
		dataUnit.id = file_id;
		dataUnit.payloadLen = dataLen;
		memcpy(dataUnit.payload, data, dataLen);
		queue_.put(dataUnit);
		ret = true;
	}

	return ret;
}
void ClientFile::remove(int file_id)
{
	{
		MutexLockGuard lock(mutex_);
		fileList_.erase(file_id);
	}
}
bool ClientFile::isWriteFileFinished(int file_id)
{
	bool ret = false;
	{
		MutexLockGuard lock(mutex_);
		if (fileList_.find(file_id) != fileList_.end())
		{
			if (fileList_[file_id]->state == kWriteFinished)
			{
				ret = true;
			}
		}
	}
	return ret;
}

void ClientFile::writeFileFunc()
{
	running_ = true;
	while (!quit_)
	{
		DataUnit dataUnit(queue_.take());
		//LOG_DEBUG;
		if (dataUnit.id == 0 || quit_)
		{
			break;
		}

		{
			FilePtr fp;
			{
				//直接下标操作,前提是必须得有。
				//因为如果不存在，直接下标操作会默认插入一个空指针。
				MutexLockGuard lock(mutex_);
				if (fileList_.find(dataUnit.id) != fileList_.end())
				{
					fp = fileList_[dataUnit.id]->ctx;
				}
			}
			if (fp)
			{

				size_t n = fwrite_unlocked(dataUnit.payload, 1, dataUnit.payloadLen, fp.get());
				//size_t n = fwrite(dataUnit.payload, 1, dataUnit.payloadLen, fp.get());
				assert(n == dataUnit.payloadLen);
			}

			{
				MutexLockGuard lock(mutex_);
				if (fileList_.find(dataUnit.id) != fileList_.end())
				{
					fileList_[dataUnit.id]->lenIndex += dataUnit.payloadLen;
					if (fileList_[dataUnit.id]->size == fileList_[dataUnit.id]->lenIndex)
					{
						fflush(fp.get());//刷新一次输出
						fileList_[dataUnit.id]->state = kWriteFinished;
						LOG_DEBUG << "write file finished.";
					}
					else
					{
						fileList_[dataUnit.id]->state = kWritting;
					}
				}
			}
		}
	}
	LOG_DEBUG;
	running_ = false;
	notRun_.notify();
}

void ClientFile::exitDownloadAndClose()
{
	quit_ = true;
	DataUnit dataUnit;
	dataUnit.id = 0;
	dataUnit.payloadLen = 0;
	queue_.put(dataUnit);

	{
		MutexLockGuard lock(mutex_);
		while (running_)
		{
			notRun_.wait();
			//notRun_.waitForSeconds(3);//确定不再操作FileInfoPtr
		}
		//注意生命周期问题：FileInfoPtr
		LOG_DEBUG;
		fileList_.clear();
		LOG_DEBUG;
	}
}



FileServer::FileServer(EventLoop *loop,
					   const InetAddress& listenAddr,
					   int maxConnections,
					   const std::string& name)
					   : loop_(loop)
					   , kMaxConnections_(maxConnections)
					   , numConnected_(0)
					   , server_(loop, listenAddr, name)
					   , dispatcher_(std::bind(&FileServer::onUnknowMessage, this, _1, _2, _3))//注册一个无法识别的默认的回调
					   //还可以注册一个解析出错的用户回调
					   , codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3), NULL)
{

	//注册收到确切protobuf类型的消息回调
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
	
	//每个连接对应线程池里的一个任务
	pool_.setMaxQueueSize(kMaxConnections_);
	pool_.start(kMaxConnections_);//设定线程池大小

}


void FileServer::start()
{
	server_.start();
}
void FileServer::setThreadNumb(int numb)
{
	//开启多线程模式
	server_.setThreadNum(numb);
}

void FileServer::onConnection(const TcpConnectionPtr& conn)
{
	LOG_INFO << conn->localAddress().toIpPort() << " -> "
		<< conn->peerAddress().toIpPort() << " is "
		<< (conn->connected() ? "UP" : "DOWN");

	ClientFilePtr getObjPtr;

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
			//构造一个客户文件对象，并与TcpConnectionPtr绑定
			//注意ClientFile为含有禁止拷贝对象
			//注意：这里是否可以使用std::ref来包装对象的引用，然后再传递
			//让boost::Any存储对象引用的拷贝,
			//当然这得保证引用被调用时对象是存在的
			{
				ClientFilePtr newObj(new edwards::ClientFile(conn->peerAddress().toIpPort()));
				assert(newObj);
				conn->setContext(newObj);
			}
			//取出每一个客户端绑定的文件对象
			getObjPtr = *boost::any_cast<ClientFilePtr>(conn->getMutableContext());
			assert(getObjPtr);

			//注意避免任务太多导致的阻塞
			LOG_DEBUG << "addTask.";
			pool_.addTask(std::bind(&ClientFile::writeFileFunc, getObjPtr));

		}

	}
	else//断开连接
	{
		--numConnected_;
		//取出每一个客户端绑定的文件对象
		 getObjPtr = *boost::any_cast<ClientFilePtr>(conn->getMutableContext());
		 if (getObjPtr)
		 {
			 getObjPtr->exitDownloadAndClose();
		 }

	}
	LOG_INFO << "numConnected = " << numConnected_;
	LOG_INFO << "clientFilePtr: " << getObjPtr.get();
}

void FileServer::onUploadStartRequest(const muduo::net::TcpConnectionPtr& conn,
									  const UploadStartRequestPtr& message,
									  muduo::Timestamp t)
{
	LOG_DEBUG << message->GetTypeName() << "\n"
		<< "pn: " << message->package_numb() << "\n"
		<< "file_id:" << message->file_id() << "\n"
		<< "file_name:" << message->file_name() << "\n"
		<< "file_size:" << message->file_size() << "\n";

	//取出每一个客户端绑定的文件对象
	ClientFilePtr getObjPtr = *boost::any_cast<ClientFilePtr>(conn->getMutableContext());
	assert(getObjPtr);
	bool ret;
	std::string result = "success";
	std::string reason = "";


	ret = getObjPtr->create(message->file_id(), message->file_name(), message->file_size());
	if (ret != true)
	{
		result = "failure";
		reason = "file info err.";		
	}

	sendStartResponse(conn, message->package_numb(), message->file_id(), result, reason);
}

void FileServer::onFileFrameTransferRequest(const muduo::net::TcpConnectionPtr& conn,
											const FileFrameTransferRequestPtr& message,
									        muduo::Timestamp t)
{
	if (message->package_numb() % 700 == 0)//每700次调用，输出一次日志
	{
		LOG_DEBUG << message->GetTypeName()
			<< "\n"
			<< "pn: " << message->package_numb() << "\n"
			<< "file_id:" << message->file_id() << "\n"
			<< "frame_size:" << message->frame_size() << "\n";
	}

		//<< message->DebugString();

	//取出每一个客户端绑定的文件对象
	ClientFilePtr getObjPtr = *boost::any_cast<ClientFilePtr>(conn->getMutableContext());
	assert(getObjPtr);
	bool ret;
	std::string result = "success";
	std::string reason = "";


	getObjPtr->appendContent(message->file_id(), message->frame_datas().c_str(), message->frame_size());

	sendFrameResponse(conn, message->package_numb(), message->file_id(), message->frame_size(), result, reason);
}



void FileServer::onUploadEndRequest(const muduo::net::TcpConnectionPtr& conn,
									const UploadEndRequestPtr& message,
									muduo::Timestamp t)
{
	LOG_DEBUG << message->GetTypeName()
		<< "pn: " << message->package_numb() << "\n"
		<< "file_id:" << message->file_id() << "\n"
		<< "frame_name:" << message->file_name() << "\n";

	//取出每一个客户端绑定的文件对象
	ClientFilePtr getObjPtr = *boost::any_cast<ClientFilePtr>(conn->getMutableContext());
	assert(getObjPtr);
	bool ret;
	std::string result = "success";
	std::string reason = "";

	if (getObjPtr->isWriteFileFinished(message->file_id()))//确定写完才会移除
	{
		getObjPtr->remove(message->file_id());
	}
	else
	{
		 result = "failure";
		 reason = "file is writing, or has not the file";
	}

	sendEndResponse(conn, message->package_numb(), message->file_id(), message->file_name(), result, reason);
}


void FileServer::sendStartResponse(const muduo::net::TcpConnectionPtr& conn,
								   int recvPn, 
								   int fileId, 
								   const std::string& result,
								   const std::string& reason)
{
	edwards::UploadStartResponse startResp;
	startResp.set_package_numb(recvPn);
	startResp.set_file_id(fileId);
	startResp.set_result(result);
	startResp.set_reason(reason);

	codec_.send(conn, startResp);
}

void FileServer::sendFrameResponse(const muduo::net::TcpConnectionPtr& conn,
									int recvPn,
									int fileId,
									int frameSize,
									const std::string& result,
									const std::string& reason)
{
	edwards::FileFrameTransferResponse frameResp;
	frameResp.set_package_numb(recvPn);
	frameResp.set_file_id(fileId);
	frameResp.set_frame_size(frameSize);//考虑是否有用
	frameResp.set_result(result);
	frameResp.set_reason(reason);

	codec_.send(conn, frameResp);
}

void FileServer::sendEndResponse(const muduo::net::TcpConnectionPtr& conn,
								 int recvPn,
								 int fileId,
								 const std::string& fileName,
								 const std::string& result,
								 const std::string& reason)
{
	edwards::UploadEndResponse endResp;
	endResp.set_package_numb(recvPn);
	endResp.set_file_id(fileId);
	endResp.set_file_name(fileName);//考虑是否有用
	endResp.set_result(result);
	endResp.set_reason(reason);

	codec_.send(conn, endResp);
}




