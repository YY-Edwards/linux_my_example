/**
* task threadpool  header,TaskThreadPool.h
*
* @platform: linux-4.4.0-62-generic
*
* @author: Edwards
*
* @revision time :20180301
*/

#ifndef __TASK_THREADPOOL_H
#define __TASK_THREADPOOL_H

#include <muduo/base/Thread.h>
#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Logging.h>

#include <stdio.h>
#include <unistd.h>

#include <functional>
#include <memory>
#include <vector>
#include <deque>

using namespace muduo;


namespace edwards
{
	class TaskThreadPool
	{//���̳߳�������������Ҫ������һ���������򲻶�ʱ�����̳߳����������һ�����̳߳�����߳���ȡ����ȥִ��.
	
	public:

		typedef std::function<void()> Task;//ִ���߳�Ǯ�ĳ�ʼ���ӿ�

		TaskThreadPool(const std::string& nameArg = std::string());//Ĭ�Ͽջص�
		~TaskThreadPool();

		void setThreadInitCallback(const Task& cb);
		void setMaxQueueSize(int maxSize){ maxQueueSize_ = maxSize; }

		bool isPoolFree(){ return !isTaskQueueFull(); }
		//�������ӿ�
		void addTask(const Task& cb);

		void start(int numbThread);//�ṩ�ⲿ�趨�߳������ӿ�
		void stop();//ֹͣ�̳߳���������߳�����

	private:


		void runInThread();
		bool isTaskQueueFull()const;
		Task take();

		std::string name_;
		bool running_;//�����̳߳����еı�־
		mutable MutexLock mutex_;//������const���εĺ�����ʹ��
		Condition notEmpty_;
		Condition notFull_;
		size_t maxQueueSize_;//���Ļ���������Ŀ
		std::deque<Task> queue_;//�������

		Task threadInitCallback_;
		std::vector<std::unique_ptr<Thread>> threads_;// IO�߳��б�
	};

}

#endif //__TASK_THREADPOOL_H
