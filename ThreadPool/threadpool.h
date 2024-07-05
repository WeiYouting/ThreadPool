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

const int TASK_MAX_THRESHHOLD = 2;			// 任务队列最大数量
const int THREAD_MAX_THRESHHOLD = 4;		// 线程池最大数量
const int TASK_COMMIT_TIMEOUT = 3;			// 提交任务超时时间
const int THREAD_MAX_IDLE_TIME = 60;		// 回收空闲线程时间

// 线程池支持的模式
enum class PoolMode {
	MODE_FIXED,		// 固定数量线程
	MODE_CACHED,	// 可动态增长线程
};



class Thread {
public:

	using ThreadFunc = std::function<void(int)>;

	// 线程构造
	Thread(ThreadFunc func)
		:func_(func)
		, threadId_(generateId_++)
	{}

	// 线程析构
	~Thread() = default;

	// 启动线程
	void start() {

		// 创建线程执行线程函数
		std::thread t(this->func_, this->threadId_);

		t.detach();

	}

	// 获取线程ID
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

	// 线程池构造
	// 线程池构造
	ThreadPool()
		:initThreadSize_(0)
		, taskSize_(0)
		, taskQueueMaxThreshHold_(TASK_MAX_THRESHHOLD)
		, poolMode_(PoolMode::MODE_FIXED)
		, isPoolRunning_(false)
		, threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
		, curThreadSize_(0)
	{}

	// 线程池析构
	~ThreadPool() {

		this->isPoolRunning_ = false;

		// 等待线程池所有线程返回
		std::unique_lock<std::mutex> lock(this->taskQueueMtx_);

		// 唤醒所有线程
		this->notEmpty_.notify_all();
		this->exitCond_.wait(lock, [&]()->bool {return this->threads_.size() == 0; });
	}


	// 设置线程池工作模式
	void setMode(PoolMode mode) {
		if (this->checkRunningState()) {
			return;
		}
		this->poolMode_ = mode;
	}


	// 设置任务队列上限阈值
	void setTaskQueueMaxThreshHold(int threshHold) {
		if (checkRunningState()) {
			return;
		}
		if (this->poolMode_ == PoolMode::MODE_CACHED) {
			this->taskQueueMaxThreshHold_ = threshHold;
		}
	}

	// 设置线程池cache模式下线程阈值
	void setThreadSizeThreshHold(int threshHold) {
		if (checkRunningState()) {
			return;
		}
		this->threadSizeThreshHold_ = threshHold;
	}

	// 提交任务
	// 使用可变参模板编程  让submitTask可以接收任意函数和参数
	template<typename Func,typename...Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))> {

		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RType> result = task->get_future();

		// 获取锁
		std::unique_lock<std::mutex> lock(this->taskQueueMtx_);

		// 线程通信 等待队列有空余
		bool res = this->notFull_.wait_for(lock, std::chrono::seconds(TASK_COMMIT_TIMEOUT), [&]()->bool {return this->taskQueue_.size() < (size_t)this->taskQueueMaxThreshHold_; });
		if (!res) {
			std::cerr << "task queue is full, submit task fail." << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()> > (
				[]()->RType {return RType(); });
			(*task)();
			return task->get_future();
		}

		// 有空余，把任务放入队列
		//this->taskQueue_.emplace(sp);
		this->taskQueue_.emplace(
			[task]() {
				(*task)();
			});
		++this->taskSize_;

		// 通知
		notEmpty_.notify_all();

		// cache模式 根据需要任务数量和空闲线程数量判断是否需要新增线程
		if (this->poolMode_ == PoolMode::MODE_CACHED) {
			if (this->taskSize_ > this->idleThreadSize_ && this->curThreadSize_ < this->threadSizeThreshHold_) {
				// 创建新线程
				std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1));
				//threads_.emplace_back(std::move(ptr));
				int threadId = ptr->getThreadId();
				this->threads_.emplace(threadId, std::move(ptr));
				// 启动线程 
				this->threads_[threadId]->start();
				this->curThreadSize_++;
				this->idleThreadSize_++;
				std::cout << "create new thread..." << std::endl;
			}
		}

		return result;
	}


	// 开启线程池
	void start(int initThreadSize = std::thread::hardware_concurrency()) {

		// 记录初始线程数量
		this->initThreadSize_ = initThreadSize;
		this->curThreadSize_ = initThreadSize;

		// 设置线程池运行状态
		this->isPoolRunning_ = true;


		// 创建线程对象
		for (int i = 0; i < this->initThreadSize_; ++i) {
			std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1));
			//this->threads_.emplace_back(std::move(ptr));
			this->threads_.emplace(ptr->getThreadId(), std::move(ptr));
		}

		// 启动所有线程
		for (int i = 0; i < this->initThreadSize_; ++i) {
			this->threads_[i]->start();		// 执行线程函数
			this->idleThreadSize_++;		// 记录初始空闲线程数量
		}

	}
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	// 自定义线程函数
	void threadHandler(int threadId) {

		//std::cout << "begin:" << std::this_thread::get_id() << std::endl;
		//std::cout << "end:" << std::this_thread::get_id() << std::endl;

		auto lastTime = std::chrono::high_resolution_clock().now();

		while (true) {
			Task task;
			{
				// 获取锁
				std::unique_lock<std::mutex> lock(this->taskQueueMtx_);
				//std::cout << "tid:" << std::this_thread::get_id() << "尝试获取任务" << std::endl;

				// cache模式下 回收空闲时间过长的多余线程
				while (this->taskQueue_.size() == 0) {

					if (!this->isPoolRunning_) {
						this->threads_.erase(threadId);
						std::cout << "thread:" << std::this_thread::get_id() << "exit" << std::endl;
						this->exitCond_.notify_all();
						return;
					}

					if (this->poolMode_ == PoolMode::MODE_CACHED) {
						if (std::cv_status::timeout == this->notEmpty_.wait_for(lock, std::chrono::seconds(1))) {
							// 判断空闲时间是否
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() >= THREAD_MAX_IDLE_TIME && this->curThreadSize_ > this->initThreadSize_) {
								// 回收线程
								this->threads_.erase(threadId);
								this->curThreadSize_--;
								this->idleThreadSize_--;
								std::cout << "thread:" << std::this_thread::get_id() << "exit" << std::endl;
								return;
							}
						}
					}
					else {
						// 等待notEmpty条件
						notEmpty_.wait(lock);
					}
					// 线程池结束 回收线程资源

				}


				//std::cout << "tid:" << std::this_thread::get_id() << "获取任务成功" << std::endl;
				this->idleThreadSize_--;

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
				task();
			}

			this->idleThreadSize_++;
			lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完任务的时间

		}




	}
	// 检查线程池运行状态
	bool checkRunningState() const{
	return this->isPoolRunning_;
}


private:
	//std::vector<std::unique_ptr<Thread>> threads_;				// 线程列表
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;		// 线程列表
	int initThreadSize_;				// 初始线程数量
	std::atomic_int curThreadSize_;		// 记录当前线程数量
	std::atomic_int idleThreadSize_;	// 空闲线程数量
	int threadSizeThreshHold_;			// 线程数量上限阈值


	using Task = std::function<void()>;
	std::queue<Task> taskQueue_;		// 任务队列
	std::atomic_int taskSize_;			// 任务数量
	int taskQueueMaxThreshHold_;		// 任务队列数量上限阈值


	std::mutex taskQueueMtx_;			// 保证任务队列线程安全
	std::condition_variable notFull_;	// 表示任务队列不满
	std::condition_variable notEmpty_;	// 表示任务队列不空
	std::condition_variable exitCond_;  // 等待线程资源全部回收


	PoolMode poolMode_;					// 当前线程池工作模式
	std::atomic_bool isPoolRunning_;	// 当前线程池的工作状态

};


#endif
