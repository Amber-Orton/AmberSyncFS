#ifndef TIMER_SET_H
#define TIMER_SET_H

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <atomic>
#include "command.h"

struct event_type_and_path {
    CommandType event_type;
    std::string relative_path;

	bool operator==(const event_type_and_path& other) const {
		return event_type == other.event_type && relative_path == other.relative_path;
	}
};

namespace std {
	template <>
	struct hash<event_type_and_path> {
		std::size_t operator()(const event_type_and_path& k) const {
			return (std::hash<int>()(static_cast<int>(k.event_type)) ^ (std::hash<std::string>()(k.relative_path) << 1));
		}
	};
}

struct access_info {
	std::chrono::steady_clock::time_point last_access_time;
	std::chrono::steady_clock::time_point last_event_creation_time;
	uint64_t mod_time;
	unsigned int access_count;
};

struct timer_set {
	std::unordered_map<event_type_and_path, access_info> myset;
	static std::atomic<bool> cleanup_thread_running;
	static std::atomic<bool> cleanup_thread_run_again;

	// Checks if an event should be created. Returns 0 if yes, 1 if not needed.
	int check(const CommandType& event_type, const std::string& relative_path, uint64_t mod_time);

	// Cleans up old entries in the timer set.
	void cleanup();

	// Thread function for cleanup. Returns nullptr.
	static void* cleanup_thread_func(void* arg);
};

extern timer_set timer_set_instance;

#endif
