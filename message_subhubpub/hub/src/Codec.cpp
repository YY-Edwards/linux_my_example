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
	//�ַ���������������"cmd <topic>\r\n..."
	Parse_Result result = kError;
	const char * crlf= buf->findCRLF();
	if (crlf)//�ҵ�������(����ж�����ҵ���һ���ͷ���):"\r\n..."
	{
		//An iterator to the first element in the range that compares equal to val.
		//If no elements match, the function returns last.
		const char * space = std::find(buf->peek(), crlf, ' ');
		if (space != crlf)//�ҵ��ո�������õ���ʼ����:" <topic>\r\n..."
		{
			//range(6)
			//template <class InputIterator>
			//string& assign(InputIterator first, InputIterator last);
			cmd->assign(buf->peek(), space);//ȥ���˿ո��Ժ�����ݣ��õ�����
			topic->assign(space + 1, crlf);
			if ((*cmd) == "pub")//Э���Ǵ����ݵ�
			{
				const char *start = crlf + 2;//��ƫ�ƣ���content\r\n��
				crlf = buf->findCRLF(start);//����������һ����\r\n��
				if (crlf)//�ҵ�������(��ʱӦ�������һ��):"\r\n"
				{
					content->assign(start, crlf);//ȥ��"\r\n"

					//buf->retrieve();//֪����������ݳ���ʱ���ô˽ӿ�
					buf->retrieveUntil(crlf+2);//��Ҫ����ĩβ��ָ�롣�������ݣ�������ָ��ƫ�ơ�
					result = kSuccess;
				}
				else
				{
					result = kContinue;//���ܻ�û�������
				}
			}
			else//sub/unsub
			{
				buf->retrieveUntil(crlf + 2);//��Ҫ����ĩβ��ָ�롣�������ݣ�������ָ��ƫ�ơ�
				result = kSuccess;
			}

		}
		else//������ĩβ��δ�ҵ�
		{
			result = kError;//Э�������ֱ�ӹرնԶ�
		}


	}
	else//Э�鲻����
	{
		result = kContinue;
	}

	return result;
}



