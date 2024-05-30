#include "threadpool.h"
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHHOLD = 1024;

// �̳߳ع���
ThreadPool::ThreadPool()
	:initThreadSize_(0)
	, taskSize_(0)
	, taskQueueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
{}

// �̳߳�����
ThreadPool::~ThreadPool() {

}


// �����̳߳ع���ģʽ
void ThreadPool::setMode(PoolMode mode) {
	this->poolMode_ = mode;
}

// �����������������ֵ
void ThreadPool::setTaskQueueMaxThreshHold(int threshHold) {
	this->taskQueueMaxThreshHold_ = threshHold;
}

// �ύ����
void ThreadPool::submitTask(std::shared_ptr<Task> sp) {

}

// �����̳߳�
void ThreadPool::start(int initThreadSize) {
	// ��¼��ʼ�߳�����
	this->initThreadSize_ = initThreadSize;

	// �����̶߳���
	for (int i = 0; i < this->initThreadSize_; ++i) {
		this->threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadHandler, this)));
	}

	// ���������߳�
	for (int i = 0; i < this->initThreadSize_; ++i) {
		this->threads_[i]->start();
	}

}

void ThreadPool::threadHandler() {

	std::cout << "begin:" << std::this_thread::get_id() << std::endl;
	std::cout << "end:" << std::this_thread::get_id() << std::endl;

}


/**************** �̷߳���ʵ�� **********************/

// �̹߳���
Thread::Thread(ThreadFunc func)
	:func_(func)
{}

// �߳�����
Thread::~Thread() {

}

// �����߳�
void Thread::start() {

	// �����߳�ִ���̺߳���
	std::thread t(this->func_);
	t.detach();

}