
#include "TaskThreadPool.h"
#include <muduo/base/Exception.h>

using namespace edwards;

TaskThreadPool::TaskThreadPool(const std::string& nameArg)
					  : name_(nameArg)
					  , running_(false)
					  , mutex_()
					  , maxQueueSize_(0)
					  , notEmpty_(mutex_)
					  , notFull_(mutex_)//��������ʹ��ͬһ������

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

	mutex_.assertLocked();//ȷ��������Ϲ���
	return maxQueueSize_ > 0 && queue_.size() >= maxQueueSize_;

}

void TaskThreadPool::addTask(const Task& task)
{//�����Ƿ���Ҫ��������

	if (threads_.empty())//����ǵ��߳�,��ֱ��ִ��
	{
		task();
	}
	else
	{		
		MutexLockGuard lock(mutex_);
		while (isTaskQueueFull())//������������������ȴ�
		{
			notFull_.wait();
		}
			
		//�еط��������
		queue_.push_back(task);//���������������
		notEmpty_.notify();//��֪ͨ��������Ҫִ��

	}

}

TaskThreadPool::Task TaskThreadPool::take()
{
	MutexLockGuard lock(mutex_);
	while (queue_.empty() && running_)
	{
		notEmpty_.wait();//�������ѭ����ȴ�
	}

	Task task;
	if (!queue_.empty())//������
	{
		task = queue_.front();//ȡ������
		queue_.pop_front();//ɾ�����������񸱱�
		if (maxQueueSize_ > 0)
		{
			notFull_.notify();//֪ͨ���пռ��������
		}
	}
	return task;//��������
}

void TaskThreadPool::stop()
{

	{
		MutexLockGuard lock(mutex_);
		running_ = false;
		notEmpty_.notifyAll();//֪ͨ���еȴ����߳��˳�
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
	threads_.reserve(numbThreads);//���������ռ��С
	running_ = true;
	for (int i = 0; i < numbThreads; i++)
	{
		char id[32];
		snprintf(id, sizeof id, "%d", (i + 1));

		//std::unique_ptr<Thread> p(new Thread(std::bind(&TaskThreadPool::threadFunc, this), name_ + id));
		//threads_.push_back(std::move(p));//��������Ȩ

		//�����ǲ������ġ���������ռ
		threads_.emplace_back(new Thread(std::bind(&TaskThreadPool::runInThread, this), name_ + id));

		threads_[i]->start();//�����̳߳�����߳�
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
		if (threadInitCallback_)//�����ǰ�ó�ʼ����������ִ�г�ʼ������
		{
			threadInitCallback_();
		}

		while (running_)//�ж��Ƿ��˳�ѭ��
		{
			Task task(take());//ȡ�����񲢹��죬���Ϊ�գ���ȴ�
			if (task)//����Ϊ��
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
