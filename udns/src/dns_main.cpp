/**
* UDNS parse main, dns_main.cpp
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180128
*/


#include "UDNSResolver.h"
#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

#include <stdio.h>


using namespace muduo;
using namespace muduo::net;

EventLoop* g_loop;

void quit()
{
	g_loop->quit();
}


//param:
//	host:domain name;
//	addr:ip infomation.
void resolveCallback(const string& host, const InetAddress& addr)
{
	LOG_INFO << "resolved" << host << " -> " << addr.toIp();

}



void resolve(UDNSResolver* res, const string& host)
{
	//注意：
	//这里是function搭配bing库的一种用法，把bing表达式作为回调函数，把不符合函数签名式的函数
	//bind转换为可接受的形式。
	//
	res->resolve(host, std::bind(&resolveCallback, host, _1));

}


int main(int argc, char* argv[])
{

	EventLoop loop;
	g_loop = &loop;
	//注意查看这里的quit()使用情况。
	loop.runAfter(20.0, quit);

	UDNSResolver udnsresolve(&loop);
	udnsresolve.start();

	resolve(&udnsresolve, "www.chenshuo.com");
	resolve(&udnsresolve, "www.example.com");
	resolve(&udnsresolve, "www.google.com");

	loop.loop();
}




