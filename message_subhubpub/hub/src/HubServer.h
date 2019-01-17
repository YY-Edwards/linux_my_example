
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

//����ʽ����������ġ���һ������
typedef std::set<string> ConnectionSubscriptionContainer;

typedef std::set<TcpConnectionPtr> ConnectionList;

//// �ֲ߳̾�����������ÿ���߳�(loop)����һ��connections_(�����б�)ʵ��
typedef ThreadLocalSingleton<ConnectionList> LocalConnections;

class Topic : public muduo::copyable
{
public:
	//const:������ָ���ֵ����
	//��ָ����ȣ��������û��������ͼ��
	Topic(const string& topic);
	Topic(const Topic& obj);//�������캯����������ĳ�Ա������ֱ��ʹ��ϵͳ�ϳɵĿ������캯�����ɣ�Ϊʲô���벻ͨ����
	//Topic(Topic&& obj);//�ƶ����캯��

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
	//һ����������ж���ͻ�
	//Ŀ��汾��audiences_δ���������̰߳�ȫ��
	//std::set<TcpConnectionPtr>audiences_;//�����ע����������пͻ�

	AudiencesPtr audiences_;
	//audiences_�Ļ�����
	MutexLock audiencesMutex_;

};

//��ֹ��������Ҫ��Ϊ�˽�ֹ�������Դ��Ŀ�������Ϳ�����ֵ����
//��Ϊ���������ɵĿ����ӿ���ǳ������ֻ�ǿ���ָ�룬����ָ��ͬһ���ڴ�ռ䡣
//��������к���ָ���Ա���Ҷ���Ĺ��캯���з������ڴ棬��ôĬ�ϵĿ������캯�������ٷ��䣬
//��ô�ڳ���������յ�ʱ�������һ���ڴ棬���������ε�����������ڴ�й©��
//���ԶԺ���ָ���Ա�Ķ�����п���ʱ����Ҫ�Լ����忽�����캯����ʹ������Ķ���ָ���Ա���Լ����ڴ�ռ䡣
//����Ҫ�����


/*

�����ǳ����
˵���������캯�����Ͳ��ò��������ǳ������ͨ����Ĭ�����ɵĿ������캯���͸�ֵ�������ֻ�Ǽ򵥵Ľ���ֵ�ĸ��ơ�
���磺�����Person�࣬�ֶ�ֻ��int��string�������ͣ����ڿ������߸�ֵʱ����ֵ���ƴ����ĳ����Ķ����Դ����Ҳ��û���κι�����
��Դ������κβ���������Ӱ�쵽���������Ķ��󡣷�֮������Person��һ������Ϊint *����ʱ�ڿ���ʱ��ֻ�ǽ���ֵ���ƣ�
��ô����������Person�����int *��ֵ�ͺ�Դ�����int *ָ�����ͬһ��λ�á��κ�һ������Ը�ֵ���޸Ķ���Ӱ�쵽��һ�����������������ǳ������
�����ǳ������Ҫ��������е�ָ��Ͷ�̬����Ŀռ���˵�ģ���Ϊ����ָ��ֻ�Ǽ򵥵�ֵ���Ʋ����ָܷ��������Ĺ�����
�κ�һ������Ը�ָ��Ĳ�������Ӱ�쵽��һ��������ʱ�����Ҫ�ṩ�Զ��������Ŀ������캯������������Ӱ�졣ͨ����ԭ���ǣ�
	1.����ָ�����͵ĳ�Ա�����ж�̬�����ڴ�ĳ�Ա��Ӧ���ṩ�Զ���Ŀ������캯��
	2.���ṩ�������캯����ͬʱ����Ӧ�ÿ���ʵ���Զ���ĸ�ֵ�����
���ڿ������캯����ʵ��Ҫȷ�����¼��㣺
	1.����ֵ���͵ĳ�Ա����ֵ����
	2.����ָ��Ͷ�̬����Ŀռ䣬�ڿ�����Ӧ���·������ռ�
	3.���ڻ��࣬Ҫ���û�����ʵĿ�����������ɻ���Ŀ���
�ܽ�
1.�������캯���͸�ֵ���������Ϊ�Ƚ����ƣ�ȴ������ͬ�Ľ�����������캯��ʹ�����еĶ��󴴽�һ���µĶ��󣬸�ֵ������ǽ�һ�������ֵ���Ƹ���һ���Ѵ��ڵĶ��������ǵ��ÿ������캯�����Ǹ�ֵ���������Ҫ�Ƿ����µĶ��������
2.���������ǳ������������ָ���Ա���ж�̬����ռ䣬��Ӧʵ���Զ���Ŀ������캯�����ṩ�˿������캯�������Ҳʵ�ָ�ֵ�������



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

	//��ΪҪ���óɻص������Բ����ṹ�ǹ̶���ص�����һ��
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


	//TcpServer���߳�ģʽʱ��HubServer_eventloopֻ����Accept�����ӣ��������ӻ�������eventloop��ִ��IO��
	EventLoop*				loop_;
	TcpServer				server_;
	std::map<string, Topic> topics_;//�������
	
	//topics_�Ļ�����
	MutexLock topicMutex_;


	////�洢ioloop
	std::set<EventLoop*>	ioLoops_;//�Ը��̻߳���в���
	////ioloops_�Ļ�����
	MutexLock mutex_;
};




}

#endif  // HUB_SERVER_H