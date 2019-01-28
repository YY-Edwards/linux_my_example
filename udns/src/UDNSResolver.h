/**
* UDNS Resolver header,UDNSResolver.h
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180125
*/

#ifndef __UDNS_RESOLVER_H
#define __UDNS_RESOLVER_H

#include <muduo/base/Timestamp.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/noncopyable.h>

#include <functional>
#include <memory>


extern "C"
{

	struct dns_ctx;
	struct dns_rr_a4;
}

//using namespace::muduo;
//using namespace::muduo::net;

namespace muduo
{
	namespace net
	{
		//前向声明：此时，只能定义指针和引用。不能定义对象，也不能以任何方式调用对象的成员
		//1.不必在include头文件了，相对会省点编译时间。
		//2.方便的解决两种类类型互相使用的问题。针对接口编程常常会遇到这种互相使用类型的场景。
		class Channel;
		class EventLoop;


		class UDNSResolver : muduo::noncopyable
		{

		public:

			typedef std::function<void(const InetAddress&)> Callback;

			UDNSResolver(EventLoop *loop);
			UDNSResolver(EventLoop *loop, const InetAddress& nameServer);
			~UDNSResolver();

			void start();

			bool resolve(const StringPiece& hostname, const Callback& cb);

		private:

			struct QueryData
			{
				UDNSResolver*	owner;
				Callback		callback;
				QueryData(UDNSResolver* o, const Callback& cb)
					:owner(o)
					, callback(cb)
				{

				}

			};

			void onRead(Timestamp time);
			void onTimer();
			void onQueryResult(struct dns_rr_a4 *result, const Callback& cb);
			static void dns_query_a4(struct dns_ctx *ctx, struct dns_rr_a4 *result, void *data);

			EventLoop* loop_;
			dns_ctx* ctx_;
			int fd_;
			bool timerActive_;
			std::unique_ptr<Channel> channel_;

		};

	}
}


#endif  // __UDNS_RESOLVER_H



