/**
* File server implementation,FileServer.h
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

		struct UserUploadFileInfo
		{
			FilePtr			ctx;
			std::string		name;
			int				size;
			std::string		storagePath;
			FileStateCode	state;
		};

		struct DataUnit
		{
			int id;
			char payload[kPayloadSize];
			//std::vector<char> payload;
		};


		typedef std::shared_ptr<UserUploadFileInfo> FileInfoPtr;

		ClientFile();
		~ClientFile();

		void create();
		void appendContent();
		void close();

	private:

		void threadFunc();

		muduo::BlockingQueue<DataUnit>	queue_;
		std::map<int, FileInfoPtr>	fileList_;
	};



	class FileServer :noncopyable
	{
	public:




		typedef std::shared_ptr<edwards::UploadStartRequest> UploadStartRequestPtr;
		typedef std::shared_ptr<edwards::UploadStartReponse> UploadStartReponsePtr;
		typedef std::shared_ptr<edwards::FileFrameTransferRequest> FileFrameTransferRequestPtr;
		typedef std::shared_ptr<edwards::FileFrameTransferResponse> FileFrameTransferResponsePtr;
		typedef std::shared_ptr<edwards::UploadEndRequest> UploadEndRequestPtr;
		typedef std::shared_ptr<edwards::UploadEndResponse> UploadEndResponsePtr;

		std::vector<std::pair<std::pair<TcpConnectionPtr, FilePtr>, std::string>> datas_;

		explicit FileServer(EventLoop *loop,
			const InetAddress& listenAddr,
			const std::string& name)
			:loop_(loop)
			, server_(loop, listenAddr, name)
			, dispatcher_(std::bind(&FileServer::onUnknowMessage, this, _1, _2, _3))//注册一个无法识别的默认的回调
			//还可以注册一个解析出错的用户回调
			, codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3), NULL)
		{

			//注册收到确切protobuf类型的消息回调
			dispatcher_.registerMessageCallback<edwards::Query>(
				std::bind(&FileServer::onQuery, this, _1, _2, _3));
			dispatcher_.registerMessageCallback<edwards::Answer>(
				std::bind(&FileServer::onAnswer, this, _1, _2, _3));

			server_.setConnectionCallback(
				std::bind(&FileServer::onConnection, this, _1));
			server_.setMessageCallback(
				std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));

		}
		~FileServer() = default;

		void start()
		{
			server_.start();
		}


	private:



		void onQuery(const muduo::net::TcpConnectionPtr& conn,
			const QueryPtr& message,
			muduo::Timestamp t)
		{
			LOG_DEBUG << "onQuery: \n" << message->GetTypeName()
				<< message->DebugString();

			message->PrintDebugString();


			edwards::Answer answer;
			answer.set_id(message->id());
			answer.set_questioner(message->questioner());
			answer.set_answerer("Your teacher");
			answer.add_solution("Jump!");
			answer.add_solution("Win!");

			messageToSend = &answer;

			codec_.send(conn, (*messageToSend));


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
				/*		if (messageToSend)
							codec_.send(conn, *messageToSend);*/
			}
			else
			{
				//loop_->quit();
			}
		}



		EventLoop*			loop_;
		TcpServer			server_;
		ProtobufDispatcher	dispatcher_;
		ProtobufCodec		codec_;

	};
}

#endif //__TCP_FILE_SERVER_H


