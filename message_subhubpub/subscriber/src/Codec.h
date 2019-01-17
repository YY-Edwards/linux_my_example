
/**
* Hub Codec header,Codec.h
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20181218
*/
//�����ӿ��ļ����Ժ����еĽ����߼���Ӧ�÷��ڴ˽ӿ��ļ��

#ifndef __HUB_CODEC_H
#define __HUB_CODEC_H

// internal header file
//<��׼��>
//���û��Զ���⡱
//�������붨��ͬʱ����һ�𣬿����Զ���Ϊ��������
#include <muduo/base/Types.h>
#include <muduo/net/Buffer.h>

#include <boost/noncopyable.hpp>

namespace pubhubsub
{
	//using  std::string;
	using muduo::string;

	typedef enum{

		kError,
		kSuccess,
		kContinue

	}Parse_Result;
	
	//Э���ʽ���£�
	//cmd <topic>\r\n
	//����cmd <topic>\r\n<content>\r\n
	//\r\nΪ�ı��ֽ����
	//cmd��ȡ:sub;unsub;pub

	Parse_Result parseMessage(muduo::net::Buffer* buf,
								string* cmd,
								string* topic,
								string* content);


}


#endif  // HUB_CODEC_H
