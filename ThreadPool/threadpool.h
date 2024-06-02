#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <queue>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

// Any����
class Any {
public:

	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	// �����������͵�����
	template<typename T>
	Any(T data): base_(std::make_unique<Derive<T>>(data)){

	}

	// ��ȡAny����洢��data����
	template<typename T>
	T case_() {
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr) {
			throw "type is unmatch";
		}
		return pd->data_;
	}
private:
	// ��������
	class Base {
	public:
		virtual ~Base() = default;
	};
	
	// ����������
	template<typename T>
	class Derive : public Base {
	public:
		Derive(T data) :data_(data) {

		};

		T data_;
	};

private:
	// �������ָ��
	std::unique_ptr<Base> base_;
};

// ʵ���ź���
class Semaphore {
public:
	Semaphore(int limit = 0) :resLimit_(limit) {

	}
	~Semaphore() = default;

	// ��ȡ�ź�����Դ
	void wait() {
		std::unique_lock<std::mutex> lock(this->mtx_);
		
		// �ȴ��ź�������Դ
		this->cond_.wait(lock, [&]()->bool {return this->resLimit_ > 0; });
		--this->resLimit_;
	}

	// �ͷ��ź�����Դ
	void post() {
		std::unique_lock<std::mutex> lock(this->mtx_);
		++this->resLimit_;
		this->cond_.notify_all();
	}


private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};

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
	Any submitTask(std::shared_ptr<Task> sp);

	// �����̳߳�
	void start(int initThreadSize = 4);

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	void threadHandler();

private:
	std::vector<std::unique_ptr<Thread>> threads_;		// �߳��б�
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
