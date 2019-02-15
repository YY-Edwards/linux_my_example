/**
* dispatcher test implementation ,dispatcher_test.cpp
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180214
*/
#include "Dispatcher.h"

#include "query_proto2.pb.h"

#include <stdio.h>
#include <iostream>

typedef std::shared_ptr<edwards::Query> QueryPtr;
typedef std::shared_ptr<edwards::Answer> AnswerPtr;
typedef std::shared_ptr<edwards::Empty> EmptyPtr;



//���Ի���������������ָ�����͵�ת��
void test_down_pointer_cast()
{
	//ϵͳ֧�ֵ���ʽת���������ൽ�����ת��
	std::shared_ptr<google::protobuf::Message> msg(new edwards::Query);
	std::shared_ptr<edwards::Query> query(std::static_pointer_cast<edwards::Query>(msg));
	assert(msg && query);
	if (!query)
	{
		abort();
	}
}


void onQuery(const muduo::net::TcpConnectionPtr& conn,
			const QueryPtr& message,
			muduo::Timestamp t)
{
	std::cout << "onQuery: "<< message->GetTypeName() << std::endl;
	auto value = message->id();

}

void onAnswer(const muduo::net::TcpConnectionPtr& conn,
			const AnswerPtr& message,
			muduo::Timestamp t)
{
	std::cout << "onAnswer: " << message->GetTypeName() << std::endl;

}

void onEmpty(const muduo::net::TcpConnectionPtr& conn,
			const EmptyPtr& message,
			muduo::Timestamp t)
{

	std::cout << "onEmpty: " << message->GetTypeName() << std::endl;
}

void onUnkonwnMessageType(const muduo::net::TcpConnectionPtr& conn,
						const MessagePtr& message,
						muduo::Timestamp t)
{
	std::cout << "onUnkonwnMessageType: " << message->GetTypeName() << std::endl;

	
}


int main()
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	test_down_pointer_cast();
	ProtobufDisapatcher dispatcher(onUnkonwnMessageType);
	dispatcher.registerMessageCallback<edwards::Query>(onQuery);
	dispatcher.registerMessageCallback<edwards::Answer>(onAnswer);
	dispatcher.registerMessageCallback<edwards::Empty>(onEmpty);

	muduo::net::TcpConnectionPtr conn;
	muduo::Timestamp t;


	std::shared_ptr<edwards::Query> query(new edwards::Query);
	std::shared_ptr<edwards::Answer> answer(new edwards::Answer);
	std::shared_ptr<edwards::Empty> empty(new edwards::Empty);

	//��̬��
	//�β�Ϊ���࣬����ʱ���þ������͸��ݴ����ʵ�ξ���
	dispatcher.onProtobufMessage(conn, query, t);
	dispatcher.onProtobufMessage(conn, answer, t);
	dispatcher.onProtobufMessage(conn, empty, t);

	google::protobuf::ShutdownProtobufLibrary();
}