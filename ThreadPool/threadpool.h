#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <queue>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

// 任务抽象基类
class Task {
public:
	virtual void run() = 0;
};

// 线程池支持的模式
enum class PoolMode {
	MODE_FIXED,		// 固定数量线程
	MODE_CACHED,	// 可动态增长线程
};

class Thread {
public:

	using ThreadFunc = std::function<void()>;

	// 线程构造
	Thread(ThreadFunc func);

	// 线程析构
	~Thread();

	// 启动线程
	void start();
private:
	ThreadFunc func_;
};

class ThreadPool {
public:

	// 线程池构造
	ThreadPool();

	// 线程池析构
	~ThreadPool();


	// 设置线程池工作模式
	void setMode(PoolMode mode);

	// 设置任务队列上限阈值
	void setTaskQueueMaxThreshHold(int threshHold);

	// 提交任务
	void submitTask(std::shared_ptr<Task> sp);

	// 开启线程池
	void start(int initThreadSize = 4);

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	void threadHandler();

private:
	std::vector<Thread*> threads_;		// 线程列表
	int initThreadSize_;				// 初始线程数量

	std::queue<std::shared_ptr<Task>> taskQueue_;	// 任务队列
	std::atomic_int taskSize_;			// 任务数量
	int taskQueueMaxThreshHold_;		// 任务队列数量上限阈值

	std::mutex taskQueueMtx_;			// 保证任务队列线程安全
	std::condition_variable notFull_;	// 表示任务队列不满
	std::condition_variable notEmpty_;	// 表示任务队列不空

	PoolMode poolMode_;					// 当前线程池工作模式
};


#endif
