#include <unordered_map>
#include <chrono>
#include <string>
#include <pthread.h>
#include "sync_event_creator.h"

struct acess_info {
    std::chrono::steady_clock::time_point last_access_time;
    std::chrono::steady_clock::time_point last_event_creation_time;
    unsigned int acess_count;
};
struct timer_set
{
    std::unordered_map<std::string, acess_info> myset;

    // checks if an element is in the set and adds it if its not, returns 1 if present and resets elements timer, 0 otherwise and if the last event creation time was more than 60 seconds ago
    int check_and_add(const std::string& key) {
        pthread_t cleanup_thread;
        pthread_create(&cleanup_thread, nullptr, &timer_set::cleanup_thread_func, this);
        pthread_detach(cleanup_thread);
        auto item = myset.find(key);
        if (item != myset.end()) {
            auto now = std::chrono::steady_clock::now();
            item->second.last_access_time = now;
            item->second.acess_count++;
            if (now - item->second.last_event_creation_time > std::chrono::seconds(60)) {
                item->second.last_event_creation_time = now;
                item->second.acess_count = 0;
                return 0; // allow event creation if last event was created more than 60 seconds ago
            }
            return 1;
        } else {
            myset[key] = {std::chrono::steady_clock::now(), std::chrono::steady_clock::now(), 0};
            return 0;
        }
    }

    // Periodic cleanup
    void cleanup() {
        auto now = std::chrono::steady_clock::now();
        for (auto item = myset.begin(); item != myset.end(); ) {
            if (now - item->second.last_access_time > std::chrono::seconds(5)) {
                bool should_create_event = item->second.acess_count >= 1;
                std::string event_key = item->first;
                item = myset.erase(item);
                if (should_create_event) {
                    auto split_pos = event_key.find(' ');
                    if (split_pos == std::string::npos || split_pos + 1 >= event_key.size()) {
                        continue;
                    }
                    auto event_type = event_key.substr(0, split_pos);
                    auto relative_path = event_key.substr(split_pos + 1);
                    create_non_priority_event_file(event_type, relative_path);
                }
            } else {
                ++item;
            }
        }
    }

    // Static thread function for pthread_create
    static void* cleanup_thread_func(void* arg) {
        timer_set* self = static_cast<timer_set*>(arg);
        self->cleanup();
        return nullptr;
    }
};
