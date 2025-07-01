#pragma once
#ifndef THREAD_POOL_CLASS_H
#define THREAD_POOL_CLASS_H

#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>

class ThreadPool {
	std::vector<std::thread> workers;
	std::queue<std::function<void()>> tasks;
	std::mutex queue_mutex;
	std::condition_variable condition;
	bool stop=false;

public:
	ThreadPool() {}

	ThreadPool(int num) {
		for(int i=0; i<num; i++) {
			workers.emplace_back([this] {
				for(;;) {
					std::unique_lock<std::mutex> lock(queue_mutex);
					condition.wait(lock, [this] { return stop||!tasks.empty(); });
					if(stop&&tasks.empty()) return;
					auto task=std::move(tasks.front());
					tasks.pop();
					lock.unlock();
					task();
				}
			});
		}
	}

	~ThreadPool() {
		std::unique_lock<std::mutex> lock(queue_mutex);
		stop=true;
		lock.unlock();
		condition.notify_all();
		for(auto& w:workers) w.join();
	}

	template<class F>
	void enqueue(F&& task) {
		std::unique_lock<std::mutex> lock(queue_mutex);
		tasks.emplace(std::forward<F>(task));
		lock.unlock();
		condition.notify_one();
	}
};

#endif