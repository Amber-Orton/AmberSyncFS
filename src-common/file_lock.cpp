#include <string>
#include <condition_variable>
#include <set>
#include <filesystem>

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
}

void unlock_file(const std::string& path) {
    std::filesystem::path norm_path = std::filesystem::path(path).lexically_normal();
    std::string norm_str = norm_path.string();
    std::unique_lock<std::mutex> map_lock(file_lock_mutex);
    file_locks.erase(norm_str);
    file_lock_cv.notify_all();
}
