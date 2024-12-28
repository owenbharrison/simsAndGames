#pragma once
#ifndef STOPWATCH_CLASS_H
#define STOPWATCH_CLASS_H

#include <chrono>

class Stopwatch {
	std::chrono::high_resolution_clock::time_point _start, _stop;
public:

	void start() {
		_start=std::chrono::high_resolution_clock::now();
	}

	void stop() {
		_stop=std::chrono::high_resolution_clock::now();
	}

	auto getMillis() const {
		return std::chrono::duration_cast<std::chrono::milliseconds>(_stop-_start).count();
	}

	auto getMicros() const {
		return std::chrono::duration_cast<std::chrono::milliseconds>(_stop-_start).count();
	}
};
#endif//STOPWATCH_STRUCT