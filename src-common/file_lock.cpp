#include <string>
#include <condition_variable>
#include <set>
#include <filesystem>
#include <iostream>

#define FILE_COLOR "\033[1;91m" // Bright red for file_lock.cpp
#define COLOR_RESET "\033[0m"

std::mutex file_lock_mutex;
std::condition_variable file_lock_cv;
std::set<std::string> file_locks;

void lock_file(const std::string& path) {
    std::filesystem::path norm_path = std::filesystem::path(path).lexically_normal();
    std::string norm_str = norm_path.string();
    std::unique_lock<std::mutex> map_lock(file_lock_mutex);
    while (file_locks.find(norm_str) != file_locks.end()) {
        file_lock_cv.wait(map_lock);
    }
    file_locks.insert(norm_str);
    std::cout << FILE_COLOR << "Locked file: " << norm_str << COLOR_RESET << "\n";
}

void unlock_file(const std::string& path) {
    std::filesystem::path norm_path = std::filesystem::path(path).lexically_normal();
    std::string norm_str = norm_path.string();
    std::unique_lock<std::mutex> map_lock(file_lock_mutex);
    file_locks.erase(norm_str);
    file_lock_cv.notify_all();
    std::cout << FILE_COLOR << "Unlocked file: " << norm_str << COLOR_RESET << "\n";
}
