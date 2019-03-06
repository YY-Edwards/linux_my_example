/**
* File server header,FileServer.h
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180222
*/


#ifndef __TCP_FILE_SERVER_H
#define __TCP_FILE_SERVER_H

#include "Codec.h"
#include "Dispatcher.h"
#include "file_upload_proto2.pb.h"
#include "taskthreadpool/TaskThreadPool.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <muduo/base/BlockingQueue.h>
#include <muduo/base/Thread.h>

#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;


namespace edwards
{


	const int		kPayloadSize = 10 * 1024;//10k

	class ClientFile
	{
	public:

		typedef std::shared_ptr<FILE> FilePtr;

		enum FileStateCode
		{
			kWaitToWrite = 1,
			kWritting,
			kWriteFinished
		};

		struct ClientUploadFileInfo
		{
			FilePtr			ctx;
			std::string		name;
			int				lenIndex;
			int				size;
			std::string		storagePath;
			FileStateCode	state;
		};

		struct DataUnit
		{
			int id;
			int payloadLen;
			char payload[kPayloadSize];
			//std::vector<char> payload;
		};


		typedef std::shared_ptr<ClientUploadFileInfo> FileInfoPtr;

		ClientFile(const std::string& clientName);
		~ClientFile();

		bool create(int file_id, std::string fileName, int file_size);
		bool appendContent(int file_id, const char* data, int dataLen);
		void remove(int file_id);
		static void onCloseFileDescriptor(FILE *fp)
		{
			LOG_DEBUG << "close fp: " <<fp;
			fclose(fp);
		}
		bool isWriteFileFinished(int file_id);
		void writeFileFunc();
		void exitDownloadAndClose();

	private:

		bool quit_;
		bool running_;
		std::string connName_;
		std::string storagePath_;
		mutable MutexLock mutex_;//可以在const修饰的函数中使用
		Condition notRun_;

		bool isFileExisted(int file_id);
		muduo::BlockingQueue<DataUnit>	queue_;
		std::map<int, FileInfoPtr>	fileList_;
	};

	typedef std::shared_ptr<edwards::ClientFile> ClientFilePtr;


	class FileServer :noncopyable
	{
	public:


		typedef std::shared_ptr<edwards::UploadStartRequest> UploadStartRequestPtr;
		//typedef std::shared_ptr<edwards::UploadStartReponse> UploadStartReponsePtr;
		typedef std::shared_ptr<edwards::FileFrameTransferRequest> FileFrameTransferRequestPtr;
		//typedef std::shared_ptr<edwards::FileFrameTransferResponse> FileFrameTransferResponsePtr;
		typedef std::shared_ptr<edwards::UploadEndRequest> UploadEndRequestPtr;
		//typedef std::shared_ptr<edwards::UploadEndResponse> UploadEndResponsePtr;

		explicit FileServer(EventLoop *loop,
							const InetAddress& listenAddr,
							int maxConnections,
							const std::string& name);
		~FileServer() = default;

		void setThreadNumb(int numb);
		void start();


	private:



		void sendStartResponse(const muduo::net::TcpConnectionPtr& conn, 
								int recvPn, 
								int fileId, 
								const std::string& result, 
								const std::string& reason);
		void sendFrameResponse(const muduo::net::TcpConnectionPtr& conn,
								int recvPn, 
								int fileId, 
								int frameSize,
								const std::string& result,
								const std::string& reason);
		void sendEndResponse(const muduo::net::TcpConnectionPtr& conn, 
								int recvPn, 
								int fileId,
								const std::string& fileName,
								const std::string& result,
								const std::string& reason);

		void onUploadStartRequest(const muduo::net::TcpConnectionPtr& conn,
								  const UploadStartRequestPtr& message,
								  muduo::Timestamp t);

		void onFileFrameTransferRequest(const muduo::net::TcpConnectionPtr& conn,
										const FileFrameTransferRequestPtr& message,
										muduo::Timestamp t);

		void onUploadEndRequest(const muduo::net::TcpConnectionPtr& conn,
								const UploadEndRequestPtr& message,
							    muduo::Timestamp t);

		void onUnknowMessage(const TcpConnectionPtr& conn,
							 const MessagePtr& message,
							 muduo::Timestamp)
		{
			LOG_DEBUG << "onUnknowMessage: \n" << message->GetTypeName();
		}

		void onConnection(const TcpConnectionPtr& conn);



		EventLoop*			loop_;
		TcpServer			server_;
		ProtobufDispatcher	dispatcher_;
		ProtobufCodec		codec_;
		TaskThreadPool		pool_;
		const int			kMaxConnections_;
		int					numConnected_; // should be atomic_int
		std::map<int, TcpConnectionPtr> clientConns_;//多个客户
	};
}

#endif //__TCP_FILE_SERVER_H


