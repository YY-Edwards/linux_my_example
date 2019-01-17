/**
* Hub Server implementation,HubServer.cpp
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20181218
*/

#include "HubServer.h"

using namespace pubhubsub;

pubhubsub::Topic::Topic(const string& topic)
				:audiences_(new ConnectionList)//初始化空的智能指针，类型为ConnectionList
{
	assert(audiences_);
	LOG_DEBUG << "constructor Topic: ";
	LOG_DEBUG << "addr: " << audiences_.get()<<"count: " << audiences_.use_count();
	topic_ = topic;
}

pubhubsub::Topic::Topic(const Topic& obj)
	:audiences_(obj.audiences_)//增加计数引用
	, topic_(obj.topic_)
	, content_(obj.content_)
	, lastPubTime_(obj.lastPubTime_)
	//, audiencesMutex_(obj.audiencesMutex_) 禁止拷贝
{
	//if (audiences_ == NULL)
	//{
	//	//LOG_DEBUG << "new audiences_";
	//	audiences_ = boost::make_shared<ConnectionList>();	//初始化空的智能指针，类型为ConnectionList	
	//}
	//assert(audiences_);
	LOG_DEBUG << "copy constructor:[ " << obj.topic_ <<" ] ";
	LOG_DEBUG << "addr: " << audiences_.get() << "count: " << audiences_.use_count();
	//
	//topic_ = obj.topic_;
}

//pubhubsub::Topic::Topic(Topic&& obj)
//	:topic_(obj.topic_)
//	, content_(obj.content_)
//	, lastPubTime_(obj.lastPubTime_)
//	, audiences_(obj.audiences_)
//
//{
//	LOG_DEBUG << "move constructor:[ " << obj.topic_ << " ] ";
//	LOG_DEBUG << "addr: " << audiences_.get();
//}

pubhubsub::Topic::~Topic()
{
	LOG_DEBUG << "addr: " << audiences_.get();
}

void pubhubsub::Topic::add(const TcpConnectionPtr& conn)
{
	//MutexLockGuard lock(audiencesMutex_);
	//if (!audiences_.unique())//某个线程正在读
	//{
	//	//COW手法：
	//	audiences_.reset(new ConnectionList(*audiences_));//拷贝构造-->swap-->生成新的副本
	//	LOG_DEBUG<<"\r\n"
	//		<<"NOTE: COPY-ON-WRITE";
	//}
	//assert(audiences_.unique());

	////在副本上写操作
	//audiences_->insert(conn);//添加订阅本主题的客户到客户列表中

	if (lastPubTime_.valid())//如果主题有内容则立即回传
	{
		conn->send(assembleMessage());
	}
}

void pubhubsub::Topic::remove(const TcpConnectionPtr& conn)
{
	//MutexLockGuard lock(audiencesMutex_);
	//if (!audiences_.unique())//某个线程正在读
	//{
	//	//COW手法：
	//	audiences_.reset(new ConnectionList(*audiences_));//拷贝构造-->swap-->生成新的副本
	//	LOG_DEBUG << "\r\n"
	//		<< "NOTE: COPY-ON-WRITE";
	//}
	//assert(audiences_.unique());
	////在副本上写操作
	//audiences_->erase(conn);
	//audiences_.erase(conn);	
}

void pubhubsub::Topic::publish(const string& content, Timestamp& time)
{
	content_ = content;
	lastPubTime_ = time;//更新发布时间
	string message = assembleMessage();


	LOG_DEBUG << "begin";
	
	BOOST_FOREACH(auto& it, LocalConnections::instance())//在所属IO线程中遍历所有连接
	{
		ConnectionSubscriptionContainer* connSub
			= boost::any_cast<ConnectionSubscriptionContainer> (it->getMutableContext());
		//if (connSub->size() > 0)
		if (connSub->find(topic_) != connSub->end())//在每个客户端里查找所关注的主题
		{
			it->send(message);//注意，这里不是跨线程调用，至少是这样期许的。
		}
		else
		{
			LOG_DEBUG << "\r\nconnection has no the topic";
		}
	}
	LOG_DEBUG << "end";

	///*在副本上的修改不会影响读者,所以读者在遍历列表的时候,不需要mutex保护*/
	//AudiencesPtr audiences = getAudiencesList();//获得一个栈变量;缩短了读时的锁的使用
	//
	//LOG_DEBUG << "\r\n audiences.size:[ " << audiences->size()<< " ]"
	//	<<"\r\n";

	//for (ConnectionList::iterator it = audiences->begin();
	//	it != audiences->end();
	//	++it)
	//{
	//	(*it)->send(message);
	//}


	//CurrentThread::sleepUsec(5000 * 1000);//5s:test


}


pubhubsub::HubServer::HubServer(muduo::net::EventLoop* loop,
								const muduo::net::InetAddress& listenAddr)
								:loop_(loop),
								server_(loop, listenAddr, "HubSever")
{
	//在TcpServer中注册类成员函数的回调
	//如果没有移动函数，那么编译器默认会选择拷贝函数
	//server_是成员变量，HubServer析构的时候，先析构server_。
	//那么传递this不会出现线程不安全情形
	server_.setConnectionCallback(
		boost::bind(&HubServer::onConnection, this, _1));
	server_.setMessageCallback(
		boost::bind(&HubServer::onMessage, this, _1, _2, _3));

	//注册一个定时任务:3s
	//而如果有移动函数，那么肯定优选选择移动函数，因此需要链接具有移动重载函数的库文件
	//那这是不是说明，boost::bind返回的是个右值？临时值
	loop_->runEvery(3.0, boost::bind(&HubServer::timePublish, this));

}

pubhubsub::HubServer::~HubServer()
{

}

void pubhubsub::HubServer::start()
{
	//设置各个ioloop的前置回调
	server_.setThreadInitCallback(boost::bind(&HubServer::threadInitFunc, this, _1));

	//TcpServer是禁止拷贝的
	/*EventLoop::Functor f = boost::bind(&TcpServer::start, server_, _1);
	loop_->runInLoop(f);*/
	server_.start();
}

//IO线程（EventLoopThread::threadFunc()）执行前时的前回调函数
void pubhubsub::HubServer::threadInitFunc(EventLoop* loop)
{
	//判断是否已经实例化
	//第一次进入分配内存后，则不再执行此函数
	assert(LocalConnections::pointer() == NULL);
	//如果未分配，再重新分配内存
	LocalConnections::instance();
	assert(LocalConnections::pointer() != NULL);

	{
		MutexLockGuard lock(mutex_);
		ioLoops_.insert(loop);//累积每一个loop（thread）	
	}

	LOG_DEBUG << "tid=" << muduo::CurrentThread::tid()
		<< ", addr:" << &LocalConnections::instance()
		<< ", ioLoops_.size: " << ioLoops_.size();

}



void pubhubsub::HubServer::onConnection(const TcpConnectionPtr& conn)
{
	LOG_DEBUG << conn->localAddress().toIpPort()
		<< " -> "
		<< conn->peerAddress().toIpPort()
		<< " is "
		<< (conn->connected() ? "UP" : "DOWN");

	if (conn->connected())
	{
		//每个连接对象都有一个主题容器
		//一个客户可以订阅多个主题
		conn->setContext(ConnectionSubscriptionContainer());//将容器通过引用保存到context，避免值传递的消耗
		//多线程的回调时，LocalConnections会根据回调时所属线程的实例进行写操作
		LocalConnections::instance().insert(conn);
	}
	else//断开，则取消其订阅的所有主题
	{
		//any_cast用于取any中的值
		//①  boost::any_cast<Type>(anyObj); 返回的不是引用而是一个副本对象；如果类型不匹配会抛出boos::bad_any_cast异常
		//②  boost::any_cast<Type>(&anyObj); 如果类型不匹配则返回nullptr，不会抛出异常。因此，在代码过程中，我们习惯使用传引用而非传值

		//any类本身不提供对内部元素的访问接口，而是使用一个友元函数any_cast()，
		//类似转型操作符，可以取出any内部持有的对象。但是需要提前知道内部值的确切类型。

		//类似取一个已存在的指针
		//常量引用，绑定后不能更改其内容
		const ConnectionSubscriptionContainer& connSub
			= boost::any_cast<const ConnectionSubscriptionContainer&> (conn->getContext());

		//windows下可以用，linux下gcc不认识
		//for each (auto str in connSub)//取消此客户的所有订阅
		//{
		//	doUnsubscribe(conn, str);
		//}
		BOOST_FOREACH(auto str, connSub)
		{
			doUnsubscribe(conn, str);
		}

		//先取消对应TcpConnection里保存的主题容器中（容器大小不为0时）的有订阅的主题
		//再在实例中删除连接
		LocalConnections::instance().erase(conn);
	}	

	LOG_DEBUG << "tid=" << muduo::CurrentThread::tid()
		<< ", addr:" << &LocalConnections::instance();
}

void pubhubsub::HubServer::onMessage(const TcpConnectionPtr& conn,
									 Buffer* buf,
									 Timestamp receiveTime)
{
	Parse_Result result = kSuccess;

	while (result == kSuccess)
	{
		string cmd;
		string topic;
		string content;
		result = parseMessage(buf, &cmd, &topic, &content);
		if (result == kSuccess)
		{
			if (cmd == "sub")
			{
				LOG_DEBUG << conn->name()
					<< ", subscribes:"
					<< topic;

				doSubscribe(conn, topic);
			}
			else if (cmd == "unsub")
			{
				LOG_DEBUG << conn->name()
					<< ", unsubscribes:"
					<< topic;

				doUnsubscribe(conn, topic);

			}
			else if (cmd == "pub")
			{
				LOG_DEBUG << conn->name()
					<< ", publish:"
					<< topic;

				doPublish(conn->name(), topic, content, receiveTime);
			}
			else
			{
				LOG_WARN << conn->name()
					<< ", cmd err!";

				conn->shutdown();
				result = kError;
			}

		}
		else if (result == kError)
		{
			LOG_DEBUG << conn->name()
				<< "protocol err!"
				<< "shutdown.";
			conn->shutdown();
			//这里只是关闭了写操作
			//如果对端故意不关闭，则考虑是否开启手动关闭
			//conn->forceClose();
		}
		//kContinue,退出，等待下一次接收直至完整数据包
	}

}

void pubhubsub::HubServer::timePublish()
{
	Timestamp now = Timestamp::now();
	LOG_DEBUG << "\r\ntimer send internal utc_time." <<"\r\n";
	//定时发布时间主题
	doPublish("internal", "utc_time", now.toFormattedString(), now);
}

void pubhubsub::HubServer::doSubscribe(const TcpConnectionPtr& conn,
									   const string& topic)
{
	LOG_DEBUG << conn->name() << " subscribes " << topic;
	//取出Sub容器
	ConnectionSubscriptionContainer* connSub
		= boost::any_cast<ConnectionSubscriptionContainer> (conn->getMutableContext());
	//增加主题
	connSub->insert(topic);//改变容器大小，类型不能被限制
	//主要用于从topic_map里找到对应的topic,然后再使用这个topic里的add()方法，
	//将订阅同一主题的所有客户存在一个主题类里。
	getTopic(topic).add(conn);

}

void pubhubsub::HubServer::doUnsubscribe(const TcpConnectionPtr& conn,
										 const string& topic)
{
	LOG_DEBUG << conn->name() << " unsubscribes " << topic;
	//先找到对应的主题，并将取消订阅的客户从列表中删除
	getTopic(topic).remove(conn);
	//取出Sub容器
	ConnectionSubscriptionContainer* connSub
		= boost::any_cast<ConnectionSubscriptionContainer> (conn->getMutableContext());
	//删除主题
	connSub->erase(topic);//改变容器大小，类型不能被限制

}


//void pubhubsub::HubServer::distributeMessage(const string& topic, const string& content)
//{
//	LOG_DEBUG << "begin";
//
//	BOOST_FOREACH(auto& it, LocalConnections::instance())//在所属IO线程中遍历所有连接
//	{
//		ConnectionSubscriptionContainer* connSub
//			= boost::any_cast<ConnectionSubscriptionContainer> (it->getMutableContext());
//		//if (connSub->size() > 0)
//		if (connSub->find(topic) != connSub->end())//在每个客户端里查找所关注的主题
//		{
//
//			//it->send();
//		}
//		else
//		{
//			LOG_DEBUG << "\r\nconnection has no the topic";
//		}
//	}
//	LOG_DEBUG << "end";
//}


void pubhubsub::HubServer::doPublish(const string& source,
									 const string& topic,
									 const string& content,
									 Timestamp time)
{
	//注意此处，不操作TcpConnection，也即发布方不需要关注任何主题更新状态。
	//找到主题，组包，依次发送
	//getTopic(topic).publish(content, time);

	LOG_DEBUG << "0";
	Topic &topicObj = getTopic(topic);

	LOG_DEBUG << "4";
	//这里的参数不能传引用类型，局部变量啊，回调是异步的，当发生回调时，可能参数引用对象已经失效了。所以默认拷贝一份是合理的。
	//但是类对象的生命周期是在的。
	//但是如果就是想在bind里传引用类型的话，那就用boost::ref。因为bind只支持值传递。所以即使声明为引用类型的实参，传递过程中，形参还是实体拷贝。
	EventLoop::Functor f = boost::bind(&Topic::publish, topicObj, content, time);

	LOG_DEBUG;

	MutexLockGuard lock(mutex_);
	/*for 循环和f达到异步进行*/
	BOOST_FOREACH(auto &it, ioLoops_)//需要改变容器中的值时需要将循环变量定义为引用类型
	{
		it->queueInLoop(f);//在所有的loop(pthread)中注册同一个主题回调函数
	}
	/*for (std::set<EventLoop*>::iterator it = ioLoops_.begin();
		it != ioLoops_.end();
		++it)
	{
		(*it)->queueInLoop(f);
	}*/
	LOG_DEBUG;

}

Topic& pubhubsub::HubServer::getTopic(const string& topic)
{
	//多线程调用的时候需要上锁？
	//LOG_DEBUG << "tid=" << muduo::CurrentThread::tid();

	MutexLockGuard lock(topicMutex_);

	std::map<string, Topic>::iterator it = topics_.find(topic);
	if (it == topics_.end())//不存在，则创建
	{
		//The single element versions (1) return a pair, 
		//with its member pair::first set to an iterator pointing to 
		//either the newly inserted element or to the element with 
		//an equivalent key in the map. The pair::second element 
		//in the pair is set to true if a new element was inserted 
		//or false if an equivalent key already existed.
		//调用的是拷贝构造函数还是赋值运算符，
		//主要是看是否有新的对象实例产生。如果产生了新的对象实例，
		//那调用的就是拷贝构造函数；如果没有，那就是对已有的对象赋值，调用的是赋值运算符。
		//调用拷贝构造函数主要有以下场景：
			//对象作为函数的参数，以值传递的方式传给函数。　
			//对象作为函数的返回值，以值的方式从函数返回。
			//使用一个对象（已存在）给另一个对象初始化（未存在）。

		//如果两个已存在的对象，再进行“=”操作，那么就是调用的赋值运算符。
		/*赋值运算符操作：
		
		赋值函数则是把一个新的对象赋值给一个原有的对象，
		所以如果原来的对象中有内存分配要先把内存释放掉（因为是用新的对象来覆盖嘛，那原先分配了内存的肯定的释放掉），
		而且还要检察一下两个对象是不是同一个对象，如果是，
		不做任何操作，直接返回。（这些要点会在下面的String实现代码中体现）
		
		*/


		//Q3：什么情况下必须定义拷贝构造函数？
		//A3：当类的对象用于函数值传递时（值参数，返回类对象），拷贝构造函数会被调用。
		//如果对象复制并非简单的值拷贝，那就必须定义拷贝构造函数。例如大的堆栈数据拷贝。
		//如果定义了拷贝构造函数，那也必须重载赋值操作符。


		LOG_DEBUG<<"1";
		//Topic a(topic);//构造

		//LOG_DEBUG << "2";

		//这里使用了对象作为函数参数以及作为函数的返回值？
		//it = topics_.insert(std::make_pair(topic, a)).first;//first返回的是迭代器
															//second返回的是插入是否成功

		//LOG_DEBUG << "3";
		it = topics_.insert(std::make_pair(topic, Topic(topic))).first;
		//也就是说，只用用一个右值，或者将亡值初始化另一个对象的时候，才会调用移动构造函数。
		//而那个move语句，就是将一个左值变成一个将亡值。
		//it = topics_.insert(std::make_pair(topic, std::move(Topic(topic)))).first;


	}
	LOG_DEBUG<<"2";
	return it->second;

	
}

