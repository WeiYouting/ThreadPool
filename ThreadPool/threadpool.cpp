#include "threadpool.h"
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHHOLD = 1024;
const int TASK_COMMIT_TIMEOUT = 5;

// 线程池构造
ThreadPool::ThreadPool()
	:initThreadSize_(0)
	, taskSize_(0)
	, taskQueueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
{}

// 线程池析构
ThreadPool::~ThreadPool() {

}


// 设置线程池工作模式
void ThreadPool::setMode(PoolMode mode) {
	this->poolMode_ = mode;
}

// 设置任务队列上限阈值
void ThreadPool::setTaskQueueMaxThreshHold(int threshHold) {
	this->taskQueueMaxThreshHold_ = threshHold;
}

// 提交任务
void ThreadPool::submitTask(std::shared_ptr<Task> sp) {
	
	// 获取锁
	std::unique_lock<std::mutex> lock(this->taskQueueMtx_);

	// 线程通信 等待队列有空余
	bool res = this->notFull_.wait_for(lock, std::chrono::seconds(TASK_COMMIT_TIMEOUT), [&]()->bool {return taskQueue_.size() < (size_t)taskQueueMaxThreshHold_; });
	if (!res) {
		std::cerr << "task queue is full, submit task fial." << std::endl;
		return;
	}

	// 有空余，把任务放入队列
	this->taskQueue_.emplace(sp);
	++this->taskSize_;

	// 通知
	notEmpty_.notify_all();
}

// 开启线程池
void ThreadPool::start(int initThreadSize) {
	// 记录初始线程数量
	this->initThreadSize_ = initThreadSize;

	// 创建线程对象
	for (int i = 0; i < this->initThreadSize_; ++i) {
		std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadHandler, this));
		this->threads_.emplace_back(std::move(ptr));
	}

	// 启动所有线程
	for (int i = 0; i < this->initThreadSize_; ++i) {
		this->threads_[i]->start();
	}

}

void ThreadPool::threadHandler() {

	//std::cout << "begin:" << std::this_thread::get_id() << std::endl;
	//std::cout << "end:" << std::this_thread::get_id() << std::endl;

	while (true) {
		std::shared_ptr<Task> task;
		{
			// 获取锁
			std::unique_lock<std::mutex> lock(this->taskQueueMtx_);
			//std::cout << "tid:" << std::this_thread::get_id() << "尝试获取任务" << std::endl;
			
			// 等待notEmpty条件
			notEmpty_.wait(lock, [&]()->bool {return taskQueue_.size() > 0; });
			//std::cout << "tid:" << std::this_thread::get_id() << "获取任务成功" << std::endl;

			// 从任务队列中取出任务
			task = this->taskQueue_.front();
			this->taskQueue_.pop();
			--this->taskSize_;

			// 如果有剩余任务，通知其他线程执行任务
			if (this->taskSize_ > 0) {
				notEmpty_.notify_all();
			}

			// 取出任务进行通知
			notFull_.notify_all();
		}		// 释放锁

		if (task != nullptr) {
			// 当前线程负责执行任务
			task->run();
		}
	}


}


/**************** 线程方法实现 **********************/

// 线程构造
Thread::Thread(ThreadFunc func)
	:func_(func)
{}

// 线程析构
Thread::~Thread() {

}

// 启动线程
void Thread::start() {

	// 创建线程执行线程函数
	std::thread t(this->func_);
	t.detach();

}