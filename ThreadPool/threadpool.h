#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <iostream>
#include <queue>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <future>

const int TASK_MAX_THRESHHOLD = 2;			// ��������������
const int THREAD_MAX_THRESHHOLD = 4;		// �̳߳��������
const int TASK_COMMIT_TIMEOUT = 3;			// �ύ����ʱʱ��
const int THREAD_MAX_IDLE_TIME = 60;		// ���տ����߳�ʱ��

// �̳߳�֧�ֵ�ģʽ
enum class PoolMode {
	MODE_FIXED,		// �̶������߳�
	MODE_CACHED,	// �ɶ�̬�����߳�
};



class Thread {
public:

	using ThreadFunc = std::function<void(int)>;

	// �̹߳���
	Thread(ThreadFunc func)
		:func_(func)
		, threadId_(generateId_++)
	{}

	// �߳�����
	~Thread() = default;

	// �����߳�
	void start() {

		// �����߳�ִ���̺߳���
		std::thread t(this->func_, this->threadId_);

		t.detach();

	}

	// ��ȡ�߳�ID
	int getThreadId() const {
		return this->threadId_;
	}

private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_;
};

int Thread::generateId_ = 0;

class ThreadPool {
public:

	// �̳߳ع���
	// �̳߳ع���
	ThreadPool()
		:initThreadSize_(0)
		, taskSize_(0)
		, taskQueueMaxThreshHold_(TASK_MAX_THRESHHOLD)
		, poolMode_(PoolMode::MODE_FIXED)
		, isPoolRunning_(false)
		, threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
		, curThreadSize_(0)
	{}

	// �̳߳�����
	~ThreadPool() {

		this->isPoolRunning_ = false;

		// �ȴ��̳߳������̷߳���
		std::unique_lock<std::mutex> lock(this->taskQueueMtx_);

		// ���������߳�
		this->notEmpty_.notify_all();
		this->exitCond_.wait(lock, [&]()->bool {return this->threads_.size() == 0; });
	}


	// �����̳߳ع���ģʽ
	void setMode(PoolMode mode) {
		if (this->checkRunningState()) {
			return;
		}
		this->poolMode_ = mode;
	}


	// �����������������ֵ
	void setTaskQueueMaxThreshHold(int threshHold) {
		if (checkRunningState()) {
			return;
		}
		if (this->poolMode_ == PoolMode::MODE_CACHED) {
			this->taskQueueMaxThreshHold_ = threshHold;
		}
	}

	// �����̳߳�cacheģʽ���߳���ֵ
	void setThreadSizeThreshHold(int threshHold) {
		if (checkRunningState()) {
			return;
		}
		this->threadSizeThreshHold_ = threshHold;
	}

	// �ύ����
	// ʹ�ÿɱ��ģ����  ��submitTask���Խ������⺯���Ͳ���
	template<typename Func,typename...Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))> {

		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RType> result = task->get_future();

		// ��ȡ��
		std::unique_lock<std::mutex> lock(this->taskQueueMtx_);

		// �߳�ͨ�� �ȴ������п���
		bool res = this->notFull_.wait_for(lock, std::chrono::seconds(TASK_COMMIT_TIMEOUT), [&]()->bool {return this->taskQueue_.size() < (size_t)this->taskQueueMaxThreshHold_; });
		if (!res) {
			std::cerr << "task queue is full, submit task fail." << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()> > (
				[]()->RType {return RType(); });
			(*task)();
			return task->get_future();
		}

		// �п��࣬������������
		//this->taskQueue_.emplace(sp);
		this->taskQueue_.emplace(
			[task]() {
				(*task)();
			});
		++this->taskSize_;

		// ֪ͨ
		notEmpty_.notify_all();

		// cacheģʽ ������Ҫ���������Ϳ����߳������ж��Ƿ���Ҫ�����߳�
		if (this->poolMode_ == PoolMode::MODE_CACHED) {
			if (this->taskSize_ > this->idleThreadSize_ && this->curThreadSize_ < this->threadSizeThreshHold_) {
				// �������߳�
				std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1));
				//threads_.emplace_back(std::move(ptr));
				int threadId = ptr->getThreadId();
				this->threads_.emplace(threadId, std::move(ptr));
				// �����߳� 
				this->threads_[threadId]->start();
				this->curThreadSize_++;
				this->idleThreadSize_++;
				std::cout << "create new thread..." << std::endl;
			}
		}

		return result;
	}


	// �����̳߳�
	void start(int initThreadSize = std::thread::hardware_concurrency()) {

		// ��¼��ʼ�߳�����
		this->initThreadSize_ = initThreadSize;
		this->curThreadSize_ = initThreadSize;

		// �����̳߳�����״̬
		this->isPoolRunning_ = true;


		// �����̶߳���
		for (int i = 0; i < this->initThreadSize_; ++i) {
			std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1));
			//this->threads_.emplace_back(std::move(ptr));
			this->threads_.emplace(ptr->getThreadId(), std::move(ptr));
		}

		// ���������߳�
		for (int i = 0; i < this->initThreadSize_; ++i) {
			this->threads_[i]->start();		// ִ���̺߳���
			this->idleThreadSize_++;		// ��¼��ʼ�����߳�����
		}

	}
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	// �Զ����̺߳���
	void threadHandler(int threadId) {

		//std::cout << "begin:" << std::this_thread::get_id() << std::endl;
		//std::cout << "end:" << std::this_thread::get_id() << std::endl;

		auto lastTime = std::chrono::high_resolution_clock().now();

		while (true) {
			Task task;
			{
				// ��ȡ��
				std::unique_lock<std::mutex> lock(this->taskQueueMtx_);
				//std::cout << "tid:" << std::this_thread::get_id() << "���Ի�ȡ����" << std::endl;

				// cacheģʽ�� ���տ���ʱ������Ķ����߳�
				while (this->taskQueue_.size() == 0) {

					if (!this->isPoolRunning_) {
						this->threads_.erase(threadId);
						std::cout << "thread:" << std::this_thread::get_id() << "exit" << std::endl;
						this->exitCond_.notify_all();
						return;
					}

					if (this->poolMode_ == PoolMode::MODE_CACHED) {
						if (std::cv_status::timeout == this->notEmpty_.wait_for(lock, std::chrono::seconds(1))) {
							// �жϿ���ʱ���Ƿ�
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() >= THREAD_MAX_IDLE_TIME && this->curThreadSize_ > this->initThreadSize_) {
								// �����߳�
								this->threads_.erase(threadId);
								this->curThreadSize_--;
								this->idleThreadSize_--;
								std::cout << "thread:" << std::this_thread::get_id() << "exit" << std::endl;
								return;
							}
						}
					}
					else {
						// �ȴ�notEmpty����
						notEmpty_.wait(lock);
					}
					// �̳߳ؽ��� �����߳���Դ

				}


				//std::cout << "tid:" << std::this_thread::get_id() << "��ȡ����ɹ�" << std::endl;
				this->idleThreadSize_--;

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
				task();
			}

			this->idleThreadSize_++;
			lastTime = std::chrono::high_resolution_clock().now(); // �����߳�ִ���������ʱ��

		}




	}
	// ����̳߳�����״̬
	bool checkRunningState() const{
	return this->isPoolRunning_;
}


private:
	//std::vector<std::unique_ptr<Thread>> threads_;				// �߳��б�
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;		// �߳��б�
	int initThreadSize_;				// ��ʼ�߳�����
	std::atomic_int curThreadSize_;		// ��¼��ǰ�߳�����
	std::atomic_int idleThreadSize_;	// �����߳�����
	int threadSizeThreshHold_;			// �߳�����������ֵ


	using Task = std::function<void()>;
	std::queue<Task> taskQueue_;		// �������
	std::atomic_int taskSize_;			// ��������
	int taskQueueMaxThreshHold_;		// �����������������ֵ


	std::mutex taskQueueMtx_;			// ��֤��������̰߳�ȫ
	std::condition_variable notFull_;	// ��ʾ������в���
	std::condition_variable notEmpty_;	// ��ʾ������в���
	std::condition_variable exitCond_;  // �ȴ��߳���Դȫ������


	PoolMode poolMode_;					// ��ǰ�̳߳ع���ģʽ
	std::atomic_bool isPoolRunning_;	// ��ǰ�̳߳صĹ���״̬

};


#endif
