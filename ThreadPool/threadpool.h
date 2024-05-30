#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <queue>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

// ����������
class Task {
public:
	virtual void run() = 0;
};

// �̳߳�֧�ֵ�ģʽ
enum class PoolMode {
	MODE_FIXED,		// �̶������߳�
	MODE_CACHED,	// �ɶ�̬�����߳�
};

class Thread {
public:

	using ThreadFunc = std::function<void()>;

	// �̹߳���
	Thread(ThreadFunc func);

	// �߳�����
	~Thread();

	// �����߳�
	void start();
private:
	ThreadFunc func_;
};

class ThreadPool {
public:

	// �̳߳ع���
	ThreadPool();

	// �̳߳�����
	~ThreadPool();


	// �����̳߳ع���ģʽ
	void setMode(PoolMode mode);

	// �����������������ֵ
	void setTaskQueueMaxThreshHold(int threshHold);

	// �ύ����
	void submitTask(std::shared_ptr<Task> sp);

	// �����̳߳�
	void start(int initThreadSize = 4);

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	void threadHandler();

private:
	std::vector<Thread*> threads_;		// �߳��б�
	int initThreadSize_;				// ��ʼ�߳�����

	std::queue<std::shared_ptr<Task>> taskQueue_;	// �������
	std::atomic_int taskSize_;			// ��������
	int taskQueueMaxThreshHold_;		// �����������������ֵ

	std::mutex taskQueueMtx_;			// ��֤��������̰߳�ȫ
	std::condition_variable notFull_;	// ��ʾ������в���
	std::condition_variable notEmpty_;	// ��ʾ������в���

	PoolMode poolMode_;					// ��ǰ�̳߳ع���ģʽ
};


#endif
