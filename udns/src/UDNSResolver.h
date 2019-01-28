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
		//ǰ����������ʱ��ֻ�ܶ���ָ������á����ܶ������Ҳ�������κη�ʽ���ö���ĳ�Ա
		//1.������includeͷ�ļ��ˣ���Ի�ʡ�����ʱ�䡣
		//2.����Ľ�����������ͻ���ʹ�õ����⡣��Խӿڱ�̳������������ֻ���ʹ�����͵ĳ�����
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



