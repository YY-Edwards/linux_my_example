
/**
* Hub Codec header,Codec.h
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20181218
*/
//编解码接口文件，以后所有的解析逻辑都应该放在此接口文件里。

#ifndef __HUB_CODEC_H
#define __HUB_CODEC_H

// internal header file
//<标准库>
//“用户自定义库”
//将声明与定义同时放在一起，可以自动成为内联函数
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
	
	//协议格式如下：
	//cmd <topic>\r\n
	//或者cmd <topic>\r\n<content>\r\n
	//\r\n为文本分界符号
	//cmd可取:sub;unsub;pub

	Parse_Result parseMessage(muduo::net::Buffer* buf,
								string* cmd,
								string* topic,
								string* content);


}


#endif  // HUB_CODEC_H
