/**
* codec test implementation ,codec_test.cpp
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180214
*/

#include "Codec.h"
#include "muduo/net/Endian.h"
#include "query_proto2.pb.h"

#include <stdio.h>
#include <zlib.h> //adler32

using namespace muduo;
using namespace muduo::net;



void printbuf(const Buffer& buf)
{
	
	printf("encoded to %zd bytes\n", buf.readableBytes());
	for (size_t i = 0; i < buf.readableBytes(); ++i)
	{
		unsigned char ch = static_cast<unsigned char>(buf.peek()[i]);

		printf("%2zd:  0x%02x  %c\n", i, ch, isgraph(ch) ? ch : ' ');
	}

}

void redoCheckSum(string& data, int len)
{
	int32_t checkSum = sockets::hostToNetwork32(static_cast<int32_t>(
		::adler32(1,
		reinterpret_cast<const Bytef*>(data.c_str()),
		static_cast<int>(len - 4))));
	data[len - 4] = reinterpret_cast<const char*>(&checkSum)[0];
	data[len - 3] = reinterpret_cast<const char*>(&checkSum)[1];
	data[len - 2] = reinterpret_cast<const char*>(&checkSum)[2];
	data[len - 1] = reinterpret_cast<const char*>(&checkSum)[3];
}

void testQuery()
{
	edwards::Query query;
	query.set_id(9);
	query.set_questioner("Edwards");

	query.add_question("Running?");
	//query.set_question(1, "Running?");
	Buffer buf;
	ProtobufCodec::fillEmptyBuffer(&buf, query);
	printbuf(buf);

	//test length 
	const int32_t len = buf.readInt32();//retrieveInt32
	assert(len == static_cast<int32_t>(buf.readableBytes()));
	ProtobufCodec::ErrorCode errorCode = ProtobufCodec::kNoError;
	MessagePtr message = ProtobufCodec::parse(buf.peek(), len, &errorCode);
	assert(errorCode == ProtobufCodec::kNoError);
	assert(message != NULL);
	message->PrintDebugString();

	//assert(message->DebugString() == query.DebugString());
	std::shared_ptr<edwards::Query> newQuery = std::static_pointer_cast<edwards::Query>(message);
	assert(newQuery != NULL);

}

void testAnswer()
{

	edwards::Answer answer;
	answer.set_id(9);
	answer.set_questioner("Edwards");
	answer.set_answerer("My teacher");
	answer.add_solution("Jump!");
	answer.add_solution("Win!");
	
	Buffer buf;
	ProtobufCodec::fillEmptyBuffer(&buf, answer);
	printbuf(buf);

	//test length 
	const int32_t len = buf.readInt32();//retrieveInt32
	assert(len == static_cast<int32_t>(buf.readableBytes()));
	ProtobufCodec::ErrorCode errorCode = ProtobufCodec::kNoError;
	MessagePtr message = ProtobufCodec::parse(buf.peek(), len, &errorCode);
	assert(errorCode == ProtobufCodec::kNoError);
	assert(message !=NULL);
	message->PrintDebugString();

	std::shared_ptr<edwards::Answer> newAnswer = std::static_pointer_cast<edwards::Answer>(message);
	assert(newAnswer != NULL);
	auto value = newAnswer->id();
	auto size = newAnswer->solution_size();

}

void testEmpty()
{

	edwards::Empty empty;

	Buffer buf;
	ProtobufCodec::fillEmptyBuffer(&buf, empty);
	printbuf(buf);

	//test length 
	const int32_t len = buf.readInt32();//retrieveInt32
	assert(len == static_cast<int32_t>(buf.readableBytes()));

	ProtobufCodec::ErrorCode errorCode = ProtobufCodec::kNoError;
	MessagePtr message = ProtobufCodec::parse(buf.peek(), len, &errorCode);
	//assert(errorCode == ProtobufCodec::kNoError);
	assert(message != NULL);
	message->PrintDebugString();


}


void testBadBuffer()
{
	edwards::Empty empty;
	empty.set_id(97);

	Buffer buf;
	ProtobufCodec::fillEmptyBuffer(&buf, empty);
	printbuf(buf);

	//test length 
	const int32_t len = buf.readInt32();//retrieveInt32
	assert(len == static_cast<int32_t>(buf.readableBytes()));

	{
		std::string data(buf.peek(), len);
		ProtobufCodec::ErrorCode errorCode = ProtobufCodec::kNoError;
		MessagePtr message = ProtobufCodec::parse(data.c_str(), len-1, &errorCode);
		assert(message == NULL);
		assert(errorCode == ProtobufCodec::kCheckSumError);
	}

	{
		std::string data(buf.peek(), len);
		ProtobufCodec::ErrorCode errorCode = ProtobufCodec::kNoError;
		data[len - 1]++;//改变校验数据值

		MessagePtr message = ProtobufCodec::parse(data.c_str(), len, &errorCode);
		assert(message == NULL);
		assert(errorCode == ProtobufCodec::kCheckSumError);
	}

	{
		std::string data(buf.peek(), len);
		ProtobufCodec::ErrorCode errorCode = ProtobufCodec::kNoError;
		data[0]++;//改变类型名长度：变大

		MessagePtr message = ProtobufCodec::parse(data.c_str(), len, &errorCode);
		assert(message == NULL);
		assert(errorCode == ProtobufCodec::kCheckSumError);
	}

	{
		std::string data(buf.peek(), len);
		ProtobufCodec::ErrorCode errorCode = ProtobufCodec::kNoError;
		data[3] = 0;//改变类型名长度：变小
		redoCheckSum(data, len);//重新计算校验
		MessagePtr message = ProtobufCodec::parse(data.c_str(), len, &errorCode);
		assert(message == NULL);
		assert(errorCode == ProtobufCodec::kInvalidNameLen);
	}

	{
		std::string data(buf.peek(), len);
		ProtobufCodec::ErrorCode errorCode = ProtobufCodec::kNoError;
		data[3] = 100;//改变类型名长度：变大
		redoCheckSum(data, len);//重新计算校验
		MessagePtr message = ProtobufCodec::parse(data.c_str(), len, &errorCode);
		assert(message == NULL);
		assert(errorCode == ProtobufCodec::kInvalidNameLen);
	}

	{
		std::string data(buf.peek(), len);
		ProtobufCodec::ErrorCode errorCode = ProtobufCodec::kNoError;
		data[3]--;//改变类型名长度：变小
		redoCheckSum(data, len);//重新计算校验
		MessagePtr message = ProtobufCodec::parse(data.c_str(), len, &errorCode);
		assert(message == NULL);
		assert(errorCode == ProtobufCodec::kUnkownMessageType);
	}

	{
		std::string data(buf.peek(), len);
		ProtobufCodec::ErrorCode errorCode = ProtobufCodec::kNoError;
		data[4] = 'M';//改变类型名长度
		redoCheckSum(data, len);//重新计算校验
		MessagePtr message = ProtobufCodec::parse(data.c_str(), len, &errorCode);
		assert(message == NULL);
		assert(errorCode == ProtobufCodec::kUnkownMessageType);
	}


	  {
		  string data(buf.peek(), len);
		  ProtobufCodec::ErrorCode errorCode = ProtobufCodec::kNoError;
		  redoCheckSum(data, len);
		  MessagePtr message = ProtobufCodec::parse(data.c_str(), len, &errorCode);
		  assert(message == NULL);
		  assert(errorCode == ProtobufCodec::kParseError);
	  }

}

int g_count = 0;

void userOnMessage(const muduo::net::TcpConnectionPtr& conn,
				const MessagePtr& message,
				muduo::Timestamp receiveTime)
{
	g_count++;
}

void testOnMessage()
{
	edwards::Query query;
	query.set_id(9);
	query.set_questioner("Edwards");
	query.add_question("Running?");


	Buffer buf1;
	ProtobufCodec::fillEmptyBuffer(&buf1, query);
	printbuf(buf1);

	edwards::Empty empty;
	empty.set_id(3);
	empty.set_id(1973); 

	Buffer buf2;
	ProtobufCodec::fillEmptyBuffer(&buf2, empty);
	printbuf(buf2);

	size_t totalLen = buf1.readableBytes() + buf2.readableBytes();
	Buffer all;//组成粘包形式
	all.append(buf1.peek(), buf1.readableBytes());
	all.append(buf2.peek(), buf2.readableBytes());
	assert(totalLen == all.readableBytes());

	muduo::net::TcpConnectionPtr conn;
	muduo::Timestamp t;

	ProtobufCodec codec(userOnMessage);

	for (size_t len = 0; len <= totalLen; ++len)
	{
		Buffer input;
		input.append(all.peek(), len);

		g_count = 0;
		codec.onMessage(conn, &input, t);
		//是否传输至少一个完整数据包
		int expected = len < buf1.readableBytes() ? 0 : 1;
		if (len == totalLen)expected = 2;
		assert(g_count == expected);
		(void)expected;
		printf("%2zd %d\n", len, g_count);

		//追加剩余的
		input.append(all.peek() + len, totalLen - len);
		codec.onMessage(conn, &input, t);
		assert(g_count == 2);
	}
}



int main()
{

	GOOGLE_PROTOBUF_VERIFY_VERSION;

	testQuery();
	printf("\r\n");

	testAnswer();
	printf("\r\n");

	testEmpty();
	printf("\r\n");

	testBadBuffer();
	printf("\r\n");

	testOnMessage();
	printf("\r\n");

	printf("ALL pass!!!\r\n");


	google::protobuf::ShutdownProtobufLibrary();

}