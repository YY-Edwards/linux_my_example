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
				:audiences_(new ConnectionList)//��ʼ���յ�����ָ�룬����ΪConnectionList
{
	assert(audiences_);
	LOG_DEBUG << "constructor Topic: ";
	LOG_DEBUG << "addr: " << audiences_.get()<<"count: " << audiences_.use_count();
	topic_ = topic;
}

pubhubsub::Topic::Topic(const Topic& obj)
	:audiences_(obj.audiences_)//���Ӽ�������
	, topic_(obj.topic_)
	, content_(obj.content_)
	, lastPubTime_(obj.lastPubTime_)
	//, audiencesMutex_(obj.audiencesMutex_) ��ֹ����
{
	//if (audiences_ == NULL)
	//{
	//	//LOG_DEBUG << "new audiences_";
	//	audiences_ = boost::make_shared<ConnectionList>();	//��ʼ���յ�����ָ�룬����ΪConnectionList	
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
	//if (!audiences_.unique())//ĳ���߳����ڶ�
	//{
	//	//COW�ַ���
	//	audiences_.reset(new ConnectionList(*audiences_));//��������-->swap-->�����µĸ���
	//	LOG_DEBUG<<"\r\n"
	//		<<"NOTE: COPY-ON-WRITE";
	//}
	//assert(audiences_.unique());

	////�ڸ�����д����
	//audiences_->insert(conn);//��Ӷ��ı�����Ŀͻ����ͻ��б���

	if (lastPubTime_.valid())//��������������������ش�
	{
		conn->send(assembleMessage());
	}
}

void pubhubsub::Topic::remove(const TcpConnectionPtr& conn)
{
	//MutexLockGuard lock(audiencesMutex_);
	//if (!audiences_.unique())//ĳ���߳����ڶ�
	//{
	//	//COW�ַ���
	//	audiences_.reset(new ConnectionList(*audiences_));//��������-->swap-->�����µĸ���
	//	LOG_DEBUG << "\r\n"
	//		<< "NOTE: COPY-ON-WRITE";
	//}
	//assert(audiences_.unique());
	////�ڸ�����д����
	//audiences_->erase(conn);
	//audiences_.erase(conn);	
}

void pubhubsub::Topic::publish(const string& content, Timestamp& time)
{
	content_ = content;
	lastPubTime_ = time;//���·���ʱ��
	string message = assembleMessage();


	LOG_DEBUG << "begin";
	
	BOOST_FOREACH(auto& it, LocalConnections::instance())//������IO�߳��б�����������
	{
		ConnectionSubscriptionContainer* connSub
			= boost::any_cast<ConnectionSubscriptionContainer> (it->getMutableContext());
		//if (connSub->size() > 0)
		if (connSub->find(topic_) != connSub->end())//��ÿ���ͻ������������ע������
		{
			it->send(message);//ע�⣬���ﲻ�ǿ��̵߳��ã���������������ġ�
		}
		else
		{
			LOG_DEBUG << "\r\nconnection has no the topic";
		}
	}
	LOG_DEBUG << "end";

	///*�ڸ����ϵ��޸Ĳ���Ӱ�����,���Զ����ڱ����б��ʱ��,����Ҫmutex����*/
	//AudiencesPtr audiences = getAudiencesList();//���һ��ջ����;�����˶�ʱ������ʹ��
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
	//��TcpServer��ע�����Ա�����Ļص�
	//���û���ƶ���������ô������Ĭ�ϻ�ѡ�񿽱�����
	//server_�ǳ�Ա������HubServer������ʱ��������server_��
	//��ô����this��������̲߳���ȫ����
	server_.setConnectionCallback(
		boost::bind(&HubServer::onConnection, this, _1));
	server_.setMessageCallback(
		boost::bind(&HubServer::onMessage, this, _1, _2, _3));

	//ע��һ����ʱ����:3s
	//��������ƶ���������ô�϶���ѡѡ���ƶ������������Ҫ���Ӿ����ƶ����غ����Ŀ��ļ�
	//�����ǲ���˵����boost::bind���ص��Ǹ���ֵ����ʱֵ
	loop_->runEvery(3.0, boost::bind(&HubServer::timePublish, this));

}

pubhubsub::HubServer::~HubServer()
{

}

void pubhubsub::HubServer::start()
{
	//���ø���ioloop��ǰ�ûص�
	server_.setThreadInitCallback(boost::bind(&HubServer::threadInitFunc, this, _1));

	//TcpServer�ǽ�ֹ������
	/*EventLoop::Functor f = boost::bind(&TcpServer::start, server_, _1);
	loop_->runInLoop(f);*/
	server_.start();
}

//IO�̣߳�EventLoopThread::threadFunc()��ִ��ǰʱ��ǰ�ص�����
void pubhubsub::HubServer::threadInitFunc(EventLoop* loop)
{
	//�ж��Ƿ��Ѿ�ʵ����
	//��һ�ν�������ڴ������ִ�д˺���
	assert(LocalConnections::pointer() == NULL);
	//���δ���䣬�����·����ڴ�
	LocalConnections::instance();
	assert(LocalConnections::pointer() != NULL);

	{
		MutexLockGuard lock(mutex_);
		ioLoops_.insert(loop);//�ۻ�ÿһ��loop��thread��	
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
		//ÿ�����Ӷ�����һ����������
		//һ���ͻ����Զ��Ķ������
		conn->setContext(ConnectionSubscriptionContainer());//������ͨ�����ñ��浽context������ֵ���ݵ�����
		//���̵߳Ļص�ʱ��LocalConnections����ݻص�ʱ�����̵߳�ʵ������д����
		LocalConnections::instance().insert(conn);
	}
	else//�Ͽ�����ȡ���䶩�ĵ���������
	{
		//any_cast����ȡany�е�ֵ
		//��  boost::any_cast<Type>(anyObj); ���صĲ������ö���һ����������������Ͳ�ƥ����׳�boos::bad_any_cast�쳣
		//��  boost::any_cast<Type>(&anyObj); ������Ͳ�ƥ���򷵻�nullptr�������׳��쳣����ˣ��ڴ�������У�����ϰ��ʹ�ô����ö��Ǵ�ֵ

		//any�౾���ṩ���ڲ�Ԫ�صķ��ʽӿڣ�����ʹ��һ����Ԫ����any_cast()��
		//����ת�Ͳ�����������ȡ��any�ڲ����еĶ��󡣵�����Ҫ��ǰ֪���ڲ�ֵ��ȷ�����͡�

		//����ȡһ���Ѵ��ڵ�ָ��
		//�������ã��󶨺��ܸ���������
		const ConnectionSubscriptionContainer& connSub
			= boost::any_cast<const ConnectionSubscriptionContainer&> (conn->getContext());

		//windows�¿����ã�linux��gcc����ʶ
		//for each (auto str in connSub)//ȡ���˿ͻ������ж���
		//{
		//	doUnsubscribe(conn, str);
		//}
		BOOST_FOREACH(auto str, connSub)
		{
			doUnsubscribe(conn, str);
		}

		//��ȡ����ӦTcpConnection�ﱣ������������У�������С��Ϊ0ʱ�����ж��ĵ�����
		//����ʵ����ɾ������
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
			//����ֻ�ǹر���д����
			//����Զ˹��ⲻ�رգ������Ƿ����ֶ��ر�
			//conn->forceClose();
		}
		//kContinue,�˳����ȴ���һ�ν���ֱ���������ݰ�
	}

}

void pubhubsub::HubServer::timePublish()
{
	Timestamp now = Timestamp::now();
	LOG_DEBUG << "\r\ntimer send internal utc_time." <<"\r\n";
	//��ʱ����ʱ������
	doPublish("internal", "utc_time", now.toFormattedString(), now);
}

void pubhubsub::HubServer::doSubscribe(const TcpConnectionPtr& conn,
									   const string& topic)
{
	LOG_DEBUG << conn->name() << " subscribes " << topic;
	//ȡ��Sub����
	ConnectionSubscriptionContainer* connSub
		= boost::any_cast<ConnectionSubscriptionContainer> (conn->getMutableContext());
	//��������
	connSub->insert(topic);//�ı�������С�����Ͳ��ܱ�����
	//��Ҫ���ڴ�topic_map���ҵ���Ӧ��topic,Ȼ����ʹ�����topic���add()������
	//������ͬһ��������пͻ�����һ���������
	getTopic(topic).add(conn);

}

void pubhubsub::HubServer::doUnsubscribe(const TcpConnectionPtr& conn,
										 const string& topic)
{
	LOG_DEBUG << conn->name() << " unsubscribes " << topic;
	//���ҵ���Ӧ�����⣬����ȡ�����ĵĿͻ����б���ɾ��
	getTopic(topic).remove(conn);
	//ȡ��Sub����
	ConnectionSubscriptionContainer* connSub
		= boost::any_cast<ConnectionSubscriptionContainer> (conn->getMutableContext());
	//ɾ������
	connSub->erase(topic);//�ı�������С�����Ͳ��ܱ�����

}


//void pubhubsub::HubServer::distributeMessage(const string& topic, const string& content)
//{
//	LOG_DEBUG << "begin";
//
//	BOOST_FOREACH(auto& it, LocalConnections::instance())//������IO�߳��б�����������
//	{
//		ConnectionSubscriptionContainer* connSub
//			= boost::any_cast<ConnectionSubscriptionContainer> (it->getMutableContext());
//		//if (connSub->size() > 0)
//		if (connSub->find(topic) != connSub->end())//��ÿ���ͻ������������ע������
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
	//ע��˴���������TcpConnection��Ҳ������������Ҫ��ע�κ��������״̬��
	//�ҵ����⣬��������η���
	//getTopic(topic).publish(content, time);

	LOG_DEBUG << "0";
	Topic &topicObj = getTopic(topic);

	LOG_DEBUG << "4";
	//����Ĳ������ܴ��������ͣ��ֲ����������ص����첽�ģ��������ص�ʱ�����ܲ������ö����Ѿ�ʧЧ�ˡ�����Ĭ�Ͽ���һ���Ǻ���ġ�
	//���������������������ڵġ�
	//���������������bind�ﴫ�������͵Ļ����Ǿ���boost::ref����Ϊbindֻ֧��ֵ���ݡ����Լ�ʹ����Ϊ�������͵�ʵ�Σ����ݹ����У��βλ���ʵ�忽����
	EventLoop::Functor f = boost::bind(&Topic::publish, topicObj, content, time);

	LOG_DEBUG;

	MutexLockGuard lock(mutex_);
	/*for ѭ����f�ﵽ�첽����*/
	BOOST_FOREACH(auto &it, ioLoops_)//��Ҫ�ı������е�ֵʱ��Ҫ��ѭ����������Ϊ��������
	{
		it->queueInLoop(f);//�����е�loop(pthread)��ע��ͬһ������ص�����
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
	//���̵߳��õ�ʱ����Ҫ������
	//LOG_DEBUG << "tid=" << muduo::CurrentThread::tid();

	MutexLockGuard lock(topicMutex_);

	std::map<string, Topic>::iterator it = topics_.find(topic);
	if (it == topics_.end())//�����ڣ��򴴽�
	{
		//The single element versions (1) return a pair, 
		//with its member pair::first set to an iterator pointing to 
		//either the newly inserted element or to the element with 
		//an equivalent key in the map. The pair::second element 
		//in the pair is set to true if a new element was inserted 
		//or false if an equivalent key already existed.
		//���õ��ǿ������캯�����Ǹ�ֵ�������
		//��Ҫ�ǿ��Ƿ����µĶ���ʵ������������������µĶ���ʵ����
		//�ǵ��õľ��ǿ������캯�������û�У��Ǿ��Ƕ����еĶ���ֵ�����õ��Ǹ�ֵ�������
		//���ÿ������캯����Ҫ�����³�����
			//������Ϊ�����Ĳ�������ֵ���ݵķ�ʽ������������
			//������Ϊ�����ķ���ֵ����ֵ�ķ�ʽ�Ӻ������ء�
			//ʹ��һ�������Ѵ��ڣ�����һ�������ʼ����δ���ڣ���

		//��������Ѵ��ڵĶ����ٽ��С�=����������ô���ǵ��õĸ�ֵ�������
		/*��ֵ�����������
		
		��ֵ�������ǰ�һ���µĶ���ֵ��һ��ԭ�еĶ���
		�������ԭ���Ķ��������ڴ����Ҫ�Ȱ��ڴ��ͷŵ�����Ϊ�����µĶ������������ԭ�ȷ������ڴ�Ŀ϶����ͷŵ�����
		���һ�Ҫ���һ�����������ǲ���ͬһ����������ǣ�
		�����κβ�����ֱ�ӷ��ء�����ЩҪ����������Stringʵ�ִ��������֣�
		
		*/


		//Q3��ʲô����±��붨�忽�����캯����
		//A3������Ķ������ں���ֵ����ʱ��ֵ��������������󣩣��������캯���ᱻ���á�
		//��������Ʋ��Ǽ򵥵�ֵ�������Ǿͱ��붨�忽�����캯���������Ķ�ջ���ݿ�����
		//��������˿������캯������Ҳ�������ظ�ֵ��������


		LOG_DEBUG<<"1";
		//Topic a(topic);//����

		//LOG_DEBUG << "2";

		//����ʹ���˶�����Ϊ���������Լ���Ϊ�����ķ���ֵ��
		//it = topics_.insert(std::make_pair(topic, a)).first;//first���ص��ǵ�����
															//second���ص��ǲ����Ƿ�ɹ�

		//LOG_DEBUG << "3";
		it = topics_.insert(std::make_pair(topic, Topic(topic))).first;
		//Ҳ����˵��ֻ����һ����ֵ�����߽���ֵ��ʼ����һ�������ʱ�򣬲Ż�����ƶ����캯����
		//���Ǹ�move��䣬���ǽ�һ����ֵ���һ������ֵ��
		//it = topics_.insert(std::make_pair(topic, std::move(Topic(topic)))).first;


	}
	LOG_DEBUG<<"2";
	return it->second;

	
}

