
/**
* Hub Server header,HubServer.h
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20181218
*/

#ifndef __HUB_SERVER_H
#define __HUB_SERVER_H

#include "Codec.h"

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/ThreadLocalSingleton.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

#include <map>
#include <set>
#include <stdio.h>
//#include <algorithm>

using namespace::muduo;
using namespace::muduo::net;

namespace pubhubsub
{

//关联式容器，有序的。定一个别名
typedef std::set<string> ConnectionSubscriptionContainer;

typedef std::set<TcpConnectionPtr> ConnectionList;

//// 线程局部单例变量，每个线程(loop)都有一个connections_(连接列表)实例
typedef ThreadLocalSingleton<ConnectionList> LocalConnections;

class Topic : public muduo::copyable
{
public:
	//const:引用所指向的值不变
	//与指针相比，传递引用还多了类型检查
	Topic(const string& topic);
	Topic(const Topic& obj);//拷贝构造函数：按此类的成员变量，直接使用系统合成的拷贝构造函数即可，为什么编译不通过？
	//Topic(Topic&& obj);//移动构造函数

	~Topic();

	void add(const TcpConnectionPtr& conn);

	void remove(const TcpConnectionPtr& conn);

	void publish(const string& content, Timestamp& time);

private:


	typedef boost::shared_ptr<ConnectionList> AudiencesPtr;

	AudiencesPtr getAudiencesList()
	{
		MutexLockGuard lock(audiencesMutex_);
		return audiences_;
	}

	string assembleMessage()
	{
		
		return "pub " + topic_ + "\r\n" + content_ + "\r\n";
	}

	string topic_;
	string content_;
	Timestamp lastPubTime_;
	//一个主题可以有多个客户
	//目标版本对audiences_未上锁，非线程安全的
	//std::set<TcpConnectionPtr>audiences_;//保存关注此主题的所有客户

	AudiencesPtr audiences_;
	//audiences_的互斥锁
	MutexLock audiencesMutex_;

};

//禁止拷贝：主要是为了禁止编译器自带的拷贝构造和拷贝赋值操作
//因为编译器生成的拷贝接口是浅拷贝，只是拷贝指针，让其指向同一个内存空间。
//如果对象中含有指针成员，且定义的构造函数中分配了内存，那么默认的拷贝构造函数不会再分配，
//那么在程序结束回收的时候，因分配一次内存，而析构两次的情况，导致内存泄漏。
//所以对含有指针成员的对象进行拷贝时，需要自己定义拷贝构造函数，使拷贝后的对象指针成员有自己的内存空间。
//即需要深拷贝。


/*

深拷贝、浅拷贝
说到拷贝构造函数，就不得不提深拷贝和浅拷贝。通常，默认生成的拷贝构造函数和赋值运算符，只是简单的进行值的复制。
例如：上面的Person类，字段只有int和string两种类型，这在拷贝或者赋值时进行值复制创建的出来的对象和源对象也是没有任何关联，
对源对象的任何操作都不会影响到拷贝出来的对象。反之，假如Person有一个对象为int *，这时在拷贝时还只是进行值复制，
那么创建出来的Person对象的int *的值就和源对象的int *指向的是同一个位置。任何一个对象对该值的修改都会影响到另一个对象，这种情况就是浅拷贝。
深拷贝和浅拷贝主要是针对类中的指针和动态分配的空间来说的，因为对于指针只是简单的值复制并不能分割开两个对象的关联，
任何一个对象对该指针的操作都会影响到另一个对象。这时候就需要提供自定义的深拷贝的拷贝构造函数，消除这种影响。通常的原则是：
	1.含有指针类型的成员或者有动态分配内存的成员都应该提供自定义的拷贝构造函数
	2.在提供拷贝构造函数的同时，还应该考虑实现自定义的赋值运算符
对于拷贝构造函数的实现要确保以下几点：
	1.对于值类型的成员进行值复制
	2.对于指针和动态分配的空间，在拷贝中应重新分配分配空间
	3.对于基类，要调用基类合适的拷贝方法，完成基类的拷贝
总结
1.拷贝构造函数和赋值运算符的行为比较相似，却产生不同的结果；拷贝构造函数使用已有的对象创建一个新的对象，赋值运算符是将一个对象的值复制给另一个已存在的对象。区分是调用拷贝构造函数还是赋值运算符，主要是否有新的对象产生。
2.关于深拷贝和浅拷贝。当类有指针成员或有动态分配空间，都应实现自定义的拷贝构造函数。提供了拷贝构造函数，最后也实现赋值运算符。



*/
class HubServer : boost::noncopyable
{
public:
	HubServer(muduo::net::EventLoop* loop,
			  const muduo::net::InetAddress& listenAddr);

	~HubServer();

	void start();
	void setThreadNumb(int numb)
	{
		server_.setThreadNum(numb);
	}

private:

	//因为要设置成回调，所以参数结构是固定与回调保持一致
	void onConnection(const TcpConnectionPtr& conn);
	void onMessage(const TcpConnectionPtr& conn,
					Buffer* buf,
					Timestamp receiveTime);
	void timePublish();
	void doSubscribe(const TcpConnectionPtr& conn,
					 const string& topic);
	void doUnsubscribe(const TcpConnectionPtr& conn,
					   const string& topic);
	void doPublish(const string& source,
				   const string& topic,
				   const string& content,
				   Timestamp time);
	Topic& getTopic(const string& topic);

	void threadInitFunc(EventLoop* loop);

	void distributeMessage(const string& topic, const string& content);


	//TcpServer多线程模式时，HubServer_eventloop只用来Accept新连接，而新连接会用其他eventloop来执行IO。
	EventLoop*				loop_;
	TcpServer				server_;
	std::map<string, Topic> topics_;//多个主题
	
	//topics_的互斥锁
	MutexLock topicMutex_;


	////存储ioloop
	std::set<EventLoop*>	ioLoops_;//对个线程会进行操作
	////ioloops_的互斥锁
	MutexLock mutex_;
};




}

#endif  // HUB_SERVER_H