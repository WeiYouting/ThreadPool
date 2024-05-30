#include "threadpool.h"
#include <chrono>
#include <thread>
#include <iostream>

int main() {

	ThreadPool pool;
	pool.start();

	std::this_thread::sleep_for(std::chrono::seconds(6));
	return 1;
}