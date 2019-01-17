/**
* Hub Codec implementation,Codec.cpp
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20181218
*/


#include "Codec.h"

using namespace muduo;
using namespace muduo::net;
using namespace pubhubsub;


Parse_Result pubhubsub::parseMessage(Buffer* buf,
	string* cmd,
	string* topic,
	string* content)
{
	//字符串逐层解析范本："cmd <topic>\r\n..."
	Parse_Result result = kError;
	const char * crlf= buf->findCRLF();
	if (crlf)//找到结束符(如果有多个，找到第一个就返回):"\r\n..."
	{
		//An iterator to the first element in the range that compares equal to val.
		//If no elements match, the function returns last.
		const char * space = std::find(buf->peek(), crlf, ' ');
		if (space != crlf)//找到空格符，并得到起始索引:" <topic>\r\n..."
		{
			//range(6)
			//template <class InputIterator>
			//string& assign(InputIterator first, InputIterator last);
			cmd->assign(buf->peek(), space);//去掉了空格以后的内容，得到命令
			topic->assign(space + 1, crlf);
			if ((*cmd) == "pub")//协议是带内容的
			{
				const char *start = crlf + 2;//先偏移：“content\r\n”
				crlf = buf->findCRLF(start);//继续查找下一个“\r\n”
				if (crlf)//找到结束符(此时应该是最后一个):"\r\n"
				{
					content->assign(start, crlf);//去掉"\r\n"

					//buf->retrieve();//知道具体的数据长度时，用此接口
					buf->retrieveUntil(crlf+2);//需要传入末尾的指针。用完数据，让数据指针偏移。
					result = kSuccess;
				}
				else
				{
					result = kContinue;//可能还没传输完成
				}
			}
			else//sub/unsub
			{
				buf->retrieveUntil(crlf + 2);//需要传入末尾的指针。用完数据，让数据指针偏移。
				result = kSuccess;
			}

		}
		else//检索到末尾都未找到
		{
			result = kError;//协议出差。外层直接关闭对端
		}


	}
	else//协议不完整
	{
		result = kContinue;
	}

	return result;
}



