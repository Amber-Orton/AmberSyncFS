#include "timer_set.h"
#include <pthread.h>
#include "sync_event_creator.h"
#include <iostream>
#include <thread>
#include <database.h>

timer_set timer_set_instance = timer_set();

std::atomic<bool> timer_set::cleanup_thread_running{false};
std::atomic<bool> timer_set::cleanup_thread_run_again{false};

// Checks if an element is in the set and adds it if it's not.
// Returns 1 if present and refreshed, 0 if missing or eligible for event creation.
int timer_set::check(const std::string& event_type, const std::string& relative_path, uint64_t mod_time) {
    pthread_t cleanup_thread;
    pthread_create(&cleanup_thread, nullptr, &timer_set::cleanup_thread_func, this);
    pthread_detach(cleanup_thread);

    std::string key = event_type + " " + relative_path;
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

    myset[key] = {std::chrono::steady_clock::now(), std::chrono::steady_clock::now(), mod_time, 0};
    return 0;
}

// Cleans up entries that haven't been accessed for more than 5 seconds and creates events for those that have been accessed at least once since creation.
void timer_set::cleanup() {
    auto now = std::chrono::steady_clock::now();
    for (auto item = myset.begin(); item != myset.end();) {
        auto elapsed = now - item->second.last_access_time;
        if (elapsed > std::chrono::seconds(5)) {
            bool should_create_event = item->second.access_count >= 1;
            std::string event_key = item->first;
            auto mod_time = item->second.mod_time;
            std::cout << "[timer_set::cleanup] Expiring key: '" << event_key << "', access_count=" << item->second.access_count << ", elapsed=" << std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() << "s\n";
            item = myset.erase(item);

            if (should_create_event) {
                auto split_pos = event_key.find(' ');
                if (split_pos == std::string::npos || split_pos + 1 >= event_key.size()) {
                    std::cerr << "Invalid event key format: " << event_key << "\n";
                    continue;
                }

                auto event_type = event_key.substr(0, split_pos);
                auto relative_path = event_key.substr(split_pos + 1);
                if (event_type.empty() || relative_path.empty()) {
                    std::cerr << "Invalid event key format: " << event_key << "\n";
                    continue;
                }
                std::cout << "[timer_set::cleanup] Creating event for expired key: type='" << event_type << "', path='" << relative_path << "'\n";
                create_event(event_type, relative_path, mod_time);
            } else {
                std::cout << "[timer_set::cleanup] Not creating event for expired key: '" << event_key << "' (access_count < 1)\n";
            }
        } else {
            ++item;
        }
    }
}

void* timer_set::cleanup_thread_func(void* arg) {
    if (cleanup_thread_running.exchange(true)) {
        cleanup_thread_run_again.store(true);
        return nullptr;
    }
    do {
        std::this_thread::sleep_for(std::chrono::milliseconds(5500)); // Sleep for a bit longer than the expiration time to ensure we catch expired items
        timer_set* self = static_cast<timer_set*>(arg);
        self->cleanup();
    } while (cleanup_thread_run_again.exchange(false));
    cleanup_thread_running.store(false);
    return nullptr;
}
