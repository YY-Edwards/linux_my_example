
#include "TaskThreadPool.h"
#include <muduo/base/Exception.h>

using namespace edwards;

TaskThreadPool::TaskThreadPool(const std::string& nameArg)
					  : name_(nameArg)
					  , running_(false)
					  , mutex_()
					  , maxQueueSize_(0)
					  , notEmpty_(mutex_)
					  , notFull_(mutex_)//条件向量使用同一互斥锁

{

}
TaskThreadPool::~TaskThreadPool()
{
	if (running_)
	{
		stop();
	}
}

void TaskThreadPool::setThreadInitCallback(const Task& cb)
{
	threadInitCallback_ = cb;
}


bool TaskThreadPool::isTaskQueueFull() const
{

	mutex_.assertLocked();//确保外层已上过锁
	return maxQueueSize_ > 0 && queue_.size() >= maxQueueSize_;

}

void TaskThreadPool::addTask(const Task& task)
{//考虑是否需要配置上限

	if (threads_.empty())//如果是单线程,则直接执行
	{
		task();
	}
	else
	{		
		MutexLockGuard lock(mutex_);
		while (isTaskQueueFull())//如果任务队列已满，则等待
		{
			notFull_.wait();
		}
			
		//有地方存放任务
		queue_.push_back(task);//允许阻塞多个任务？
		notEmpty_.notify();//并通知有任务需要执行

	}

}

TaskThreadPool::Task TaskThreadPool::take()
{
	MutexLockGuard lock(mutex_);
	while (queue_.empty() && running_)
	{
		notEmpty_.wait();//任务空且循环则等待
	}

	Task task;
	if (!queue_.empty())//有任务
	{
		task = queue_.front();//取出任务
		queue_.pop_front();//删除队列中任务副本
		if (maxQueueSize_ > 0)
		{
			notFull_.notify();//通知，有空间放任务了
		}
	}
	return task;//返回任务
}

void TaskThreadPool::stop()
{

	{
		MutexLockGuard lock(mutex_);
		running_ = false;
		notEmpty_.notifyAll();//通知所有等待的线程退出
	}
	for (auto& thr: threads_)
	{
		LOG_DEBUG << "exit thread: " << thr->name();
		thr->join();
		LOG_DEBUG << "exit thread okay. " ;
	}

}

void TaskThreadPool::start(int numbThreads)
{
	threads_.reserve(numbThreads);//重置容器空间大小
	running_ = true;
	for (int i = 0; i < numbThreads; i++)
	{
		char id[32];
		snprintf(id, sizeof id, "%d", (i + 1));

		//std::unique_ptr<Thread> p(new Thread(std::bind(&TaskThreadPool::threadFunc, this), name_ + id));
		//threads_.push_back(std::move(p));//放弃管理权

		//下面是不拷贝的。由容器独占
		threads_.emplace_back(new Thread(std::bind(&TaskThreadPool::runInThread, this), name_ + id));

		threads_[i]->start();//启动线程池里的线程
	}

	if (numbThreads == 0 && threadInitCallback_)
	{
		threadInitCallback_();
	}
}

void TaskThreadPool::runInThread()
{
	try
	{
		if (threadInitCallback_)//如果有前置初始化任务，则先执行初始化任务
		{
			threadInitCallback_();
		}

		while (running_)//判断是否退出循环
		{
			Task task(take());//取出任务并构造，如果为空，则等待
			if (task)//任务不为空
			{
				LOG_DEBUG <<"take task, tid= "<< muduo::CurrentThread::tid();
				task();
			}
			else
			{
				LOG_DEBUG << "task is null, and wait.";
			}
		}
	}
	catch (const Exception& ex)
	{
		fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
		fprintf(stderr, "reason: %s\n", ex.what());
		fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
		abort();
	}
	catch (const std::exception& ex)
	{
		fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
		fprintf(stderr, "reason: %s\n", ex.what());
		abort();
	}
	catch (...)
	{
		fprintf(stderr, "unknown exception caught in ThreadPool %s\n", name_.c_str());
		throw; // rethrow
	}
}
