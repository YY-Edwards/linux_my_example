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
	{//出线程池至少有两个主要动作，一个是主程序不定时地向线程池添加任务，另一个是线程池里的线程领取任务去执行.
	
	public:

		typedef std::function<void()> Task;//执行线程钱的初始化接口

		TaskThreadPool(const std::string& nameArg = std::string());//默认空回调
		~TaskThreadPool();

		void setThreadInitCallback(const Task& cb);
		void setMaxQueueSize(int maxSize){ maxQueueSize_ = maxSize; }

		bool isPoolFree(){ return !isTaskQueueFull(); }
		//添加任务接口
		void addTask(const Task& cb);

		void start(int numbThread);//提供外部设定线程数量接口
		void stop();//停止线程池里的所有线程任务

	private:


		void runInThread();
		bool isTaskQueueFull()const;
		Task take();

		std::string name_;
		bool running_;//整个线程池运行的标志
		mutable MutexLock mutex_;//可以在const修饰的函数中使用
		Condition notEmpty_;
		Condition notFull_;
		size_t maxQueueSize_;//最大的缓冲任务数目
		std::deque<Task> queue_;//任务队列

		Task threadInitCallback_;
		std::vector<std::unique_ptr<Thread>> threads_;// IO线程列表
	};

}

#endif //__TASK_THREADPOOL_H
