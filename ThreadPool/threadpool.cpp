#include "threadpool.h"
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHHOLD = 1024;
const int TASK_COMMIT_TIMEOUT = 5;

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
	
	// ��ȡ��
	std::unique_lock<std::mutex> lock(this->taskQueueMtx_);

	// �߳�ͨ�� �ȴ������п���
	bool res = this->notFull_.wait_for(lock, std::chrono::seconds(TASK_COMMIT_TIMEOUT), [&]()->bool {return taskQueue_.size() < (size_t)taskQueueMaxThreshHold_; });
	if (!res) {
		std::cerr << "task queue is full, submit task fial." << std::endl;
		return;
	}

	// �п��࣬������������
	this->taskQueue_.emplace(sp);
	++this->taskSize_;

	// ֪ͨ
	notEmpty_.notify_all();
}

// �����̳߳�
void ThreadPool::start(int initThreadSize) {
	// ��¼��ʼ�߳�����
	this->initThreadSize_ = initThreadSize;

	// �����̶߳���
	for (int i = 0; i < this->initThreadSize_; ++i) {
		std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadHandler, this));
		this->threads_.emplace_back(std::move(ptr));
	}

	// ���������߳�
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
			// ��ȡ��
			std::unique_lock<std::mutex> lock(this->taskQueueMtx_);
			//std::cout << "tid:" << std::this_thread::get_id() << "���Ի�ȡ����" << std::endl;
			
			// �ȴ�notEmpty����
			notEmpty_.wait(lock, [&]()->bool {return taskQueue_.size() > 0; });
			//std::cout << "tid:" << std::this_thread::get_id() << "��ȡ����ɹ�" << std::endl;

			// �����������ȡ������
			task = this->taskQueue_.front();
			this->taskQueue_.pop();
			--this->taskSize_;

			// �����ʣ������֪ͨ�����߳�ִ������
			if (this->taskSize_ > 0) {
				notEmpty_.notify_all();
			}

			// ȡ���������֪ͨ
			notFull_.notify_all();
		}		// �ͷ���

		if (task != nullptr) {
			// ��ǰ�̸߳���ִ������
			task->run();
		}
	}


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