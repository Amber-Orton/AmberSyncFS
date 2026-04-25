#ifndef TIMER_SET_H
#define TIMER_SET_H

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <atomic>

struct access_info {
	std::chrono::steady_clock::time_point last_access_time;
	std::chrono::steady_clock::time_point last_event_creation_time;
	uint64_t mod_time;
	unsigned int access_count;
};

struct timer_set {
	std::unordered_map<std::string, access_info> myset;
	static std::atomic<bool> cleanup_thread_running;
	static std::atomic<bool> cleanup_thread_run_again;

	// Checks if an event should be created. Returns 0 if yes, 1 if not needed.
	int check(const std::string& event_type, const std::string& relative_path, uint64_t mod_time);

	// Cleans up old entries in the timer set.
	void cleanup();

	// Thread function for cleanup. Returns nullptr.
	static void* cleanup_thread_func(void* arg);
};

extern timer_set timer_set_instance;

#endif
