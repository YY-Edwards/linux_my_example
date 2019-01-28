/**
* UDNS Resolver implementation. UDNSResolver.cpp
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180125
*/

#include "UDNSResolver.h"
#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>

#include "udns.h"
#include <assert.h>
#include <stdio.h>

namespace
{
	int init_udns()
	{
		static bool initialized = false;
		if (!initialized)//第一次调用
		{
			dns_init(NULL, 0);
		}
		else
		{
			printf("dns initialized.\r\n");
		}
		initialized = true;
		return  1;
	}

	struct UdnsInitializer
	{
		UdnsInitializer()
		{
			printf("dns init.\r\n");
			init_udns();
		}
		~UdnsInitializer()
		{
			printf("dns reset.\r\n");
			dns_reset(NULL);
		}

	};




	UdnsInitializer		udnsInitializer;//为什么要这样做？请思考
	const bool kDebug = true;
}


using namespace muduo::net;

UDNSResolver::UDNSResolver(EventLoop *loop)
	:loop_(loop)
	, ctx_(NULL)
	, fd_(-1)
	, timerActive_(false)
{
	init_udns();
	ctx_ = dns_new(NULL);
	assert(ctx_ != NULL);
	::dns_set_opt(ctx_, DNS_OPT_TIMEOUT, 2);

}

UDNSResolver::UDNSResolver(EventLoop *loop, const InetAddress& nameServer)
	:loop_(loop)
	, ctx_(NULL)
	, fd_(-1)
	, timerActive_(false)
{
	init_udns();
	ctx_ = ::dns_new(NULL);
	assert(ctx_ != NULL);
	::dns_add_serv_s(ctx_, nameServer.getSockAddr());
	::dns_set_opt(ctx_, DNS_OPT_TIMEOUT, 2);

}


UDNSResolver::~UDNSResolver()
{
	if (channel_)
	{
		channel_->disableAll();
		channel_->remove();
	}

	::dns_free(ctx_);
}

void UDNSResolver::start()
{
	fd_ = ::dns_open(ctx_);// And before opening, the context should be initialized.
	channel_.reset(new Channel(loop_, fd_));
	//只需要响应读事件即可
	channel_->setReadCallback(std::bind(&UDNSResolver::onRead, this, _1));
	channel_->enableReading();

}

void UDNSResolver::onRead(Timestamp time)
{
	LOG_DEBUG << "onRead " << time.toString();
	::dns_ioevent(ctx_, time.secondsSinceEpoch());
	
}

bool UDNSResolver::resolve(const StringPiece& hostname, const Callback& cb)
{
	loop_->assertInLoopThread();//保证本线程调用
	QueryData* queryData = new QueryData(this, cb);//申请堆内存：创建QueryData对象
	time_t now = time(NULL);
	//传入域名，解析后的回调接口，以及一个指针
	//这里的最后一个参数指向一个结构体，其中绑定了本类的对象指针以及用户的回调函数。
	//这样在UDNS回调UDNSResolver::dns_query_a4（）时才知道回调那个用户回调函数
	//回调同一个函数接口，那么通过函数传递的指针参数再区别不同的用户回调
	//最后一个参数应该传递一个堆变量，需要生命周期大于本函数而存在。

	//发起解析请求
	struct dns_query* query =
		::dns_submit_a4(ctx_, hostname.data(), 0, &UDNSResolver::dns_query_a4, queryData);

	//设置超时
	int timeout = ::dns_timeouts(ctx_, -1, now);

	LOG_DEBUG << "timeout " << timeout << " active " << timerActive_ << " " << queryData;
	if (timerActive_)
	{
		loop_->runAfter(timeout, std::bind(&UDNSResolver::onTimer, this));
		timerActive_ = true;

	}
	return query != NULL;


}

void UDNSResolver::dns_query_a4(struct dns_ctx *ctx, struct dns_rr_a4 *result, void *data)
{
	QueryData* query = static_cast<QueryData*>(data);

	assert(ctx == query->owner->ctx_);

	query->owner->onQueryResult(result, query->callback);

	free(result);
	delete query;

}


void UDNSResolver::onQueryResult(struct dns_rr_a4 *result, const Callback& callback)
{
	int status = ::dns_status(ctx_);
	LOG_DEBUG << "onQueryResult " << status;
	struct sockaddr_in addr;
	bzero(&addr, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_port = 0;
	if (result)//不为空
	{
		if (kDebug)
		{
			printf("cname %s\n", result->dnsa4_cname);
			printf("qname %s\n", result->dnsa4_qname);
			printf("ttl %d\n", result->dnsa4_ttl);
			printf("nrr %d\n", result->dnsa4_nrr);
			for (int i = 0; i < result->dnsa4_nrr; ++i)
			{
				char buf[32];
				::dns_ntop(AF_INET, &result->dnsa4_addr[i], buf, sizeof buf);
				printf("  %s\n", buf);
			}
		}
		addr.sin_addr = result->dnsa4_addr[0];
	
	}

	InetAddress inet(addr);

	LOG_DEBUG << "user callback :" << &callback;

	callback(inet);//回调用户回调

}




void UDNSResolver::onTimer()
{
	assert(timerActive_ == true);//首先确定定时被激活
	time_t now = loop_->pollReturnTime().secondsSinceEpoch();//得到数据到达的时间戳
	int timeout = ::dns_timeouts(ctx_, -1, now);//一次次的设置超时
	LOG_DEBUG << "onTimer " << loop_->pollReturnTime().toString()
		<< " timeout " << timeout;

	if (timeout < 0)//无挂起事件
	{
		timerActive_ = false;
	}
	else//仍然未处理请求，再次定时回调
	{
		loop_->runAfter(timeout, std::bind(&UDNSResolver::onTimer, this));
	}

	
}