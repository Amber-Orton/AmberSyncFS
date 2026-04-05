#include "timer_set.h"
#include <pthread.h>
#include "sync_event_creator.h"
#include <iostream>

timer_set timer_set_instance = timer_set();

std::atomic<bool> timer_set::cleanup_thread_running{false};

// Checks if an element is in the set and adds it if it's not.
// Returns 1 if present and refreshed, 0 if missing or eligible for event creation.
int timer_set::check(const std::string& event_type, const std::string& event_priority, const std::string& relative_path) {
    pthread_t cleanup_thread;
    pthread_create(&cleanup_thread, nullptr, &timer_set::cleanup_thread_func, this);
    pthread_detach(cleanup_thread);

    std::string key = event_type + " " + event_priority + " " + relative_path;
    auto item = myset.find(key);
    if (item != myset.end()) {
        auto now = std::chrono::steady_clock::now();
        item->second.last_access_time = now;
        item->second.access_count++;
        if (now - item->second.last_event_creation_time > std::chrono::seconds(60)) {
            item->second.last_event_creation_time = now;
            item->second.access_count = 0;
            return 0;
        }
        return 1;
    }

    myset[key] = {std::chrono::steady_clock::now(), std::chrono::steady_clock::now(), 0};
    return 0;
}

// Cleans up entries that haven't been accessed for more than 5 seconds and creates events for those that have been accessed at least once since creation.
void timer_set::cleanup() {
    auto now = std::chrono::steady_clock::now();
    for (auto item = myset.begin(); item != myset.end();) {
        if (now - item->second.last_access_time > std::chrono::seconds(5)) {
            bool should_create_event = item->second.access_count >= 1;
            std::string event_key = item->first;
            item = myset.erase(item);

            if (should_create_event) {
                auto first_split_pos = event_key.find(' ');
                if (first_split_pos == std::string::npos || first_split_pos + 1 >= event_key.size()) {
                    std::cerr << "Invalid event key format: " << event_key << "\n";
                    continue;
                }

                auto second_split_pos = event_key.find(' ', first_split_pos + 1);
                if (second_split_pos == std::string::npos || second_split_pos + 1 >= event_key.size()) {
                    std::cerr << "Invalid event key format: " << event_key << "\n";
                    continue;
                }

                auto event_type = event_key.substr(0, first_split_pos);
                auto event_priority = event_key.substr(first_split_pos + 1, second_split_pos - first_split_pos - 1);
                auto relative_path = event_key.substr(second_split_pos + 1);
                if (event_type.empty() || event_priority.empty() || relative_path.empty()) {
                    std::cerr << "Invalid event key format: " << event_key << "\n";
                    continue;
                }

                create_event_file(event_type, event_priority, relative_path);
            }
        } else {
            ++item;
        }
    }
}

void* timer_set::cleanup_thread_func(void* arg) {
    if (cleanup_thread_running.exchange(true)) {
        return nullptr;
    }
    timer_set* self = static_cast<timer_set*>(arg);
    self->cleanup();
    cleanup_thread_running.store(false);
    return nullptr;
}
