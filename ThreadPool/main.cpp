#include "threadpool.h"
#include <chrono>
#include <thread>
#include <iostream>

class TaskA :public Task {
public:
	void run() {
		std::this_thread::sleep_for(std::chrono::seconds(3));
		std::cout << "tid:" << std::this_thread::get_id() << "任务执行完成" << std::endl;

	}
};


int main() {

	ThreadPool pool;
	pool.start();


	pool.submitTask(std::make_shared<TaskA>());
	pool.submitTask(std::make_shared<TaskA>());
	pool.submitTask(std::make_shared<TaskA>());
	pool.submitTask(std::make_shared<TaskA>());

	pool.submitTask(std::make_shared<TaskA>());
	pool.submitTask(std::make_shared<TaskA>());
	pool.submitTask(std::make_shared<TaskA>());
	pool.submitTask(std::make_shared<TaskA>());
	pool.submitTask(std::make_shared<TaskA>());

	getchar();


	return 1;
}