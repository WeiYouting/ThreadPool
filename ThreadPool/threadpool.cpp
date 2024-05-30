#include "threadpool.h"
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHHOLD = 1024;

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

}

// 开启线程池
void ThreadPool::start(int initThreadSize) {
	// 记录初始线程数量
	this->initThreadSize_ = initThreadSize;

	// 创建线程对象
	for (int i = 0; i < this->initThreadSize_; ++i) {
		this->threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadHandler, this)));
	}

	// 启动所有线程
	for (int i = 0; i < this->initThreadSize_; ++i) {
		this->threads_[i]->start();
	}

}

void ThreadPool::threadHandler() {

	std::cout << "begin:" << std::this_thread::get_id() << std::endl;
	std::cout << "end:" << std::this_thread::get_id() << std::endl;

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