#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <atomic>

struct access_info {
	std::chrono::steady_clock::time_point last_access_time;
	std::chrono::steady_clock::time_point last_event_creation_time;
	unsigned int access_count;
};

struct timer_set {
	std::unordered_map<std::string, access_info> myset;
	static std::atomic<bool> cleanup_thread_running;
	static std::atomic<bool> cleanup_thread_run_again;

	int check(const std::string& event_type, const std::string& relative_path);
	void cleanup();
	static void* cleanup_thread_func(void* arg);
};

extern timer_set timer_set_instance;
