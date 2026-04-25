
#include <string>
#include <filesystem>

// Acquires a lock for the given file or directory path (blocks if already locked).
void lock_file(const std::string& path);

// Releases a lock for the given file or directory path.
void unlock_file(const std::string& path);

// RAII guard for file/directory locking. Acquires lock on construction, releases on destruction.
class FileLockGuard {
public:
    explicit FileLockGuard(const std::string& path)
        : norm_path_(std::filesystem::path(path).lexically_normal().string()) {
        lock_file(norm_path_);
    }
    ~FileLockGuard() { unlock_file(norm_path_); }
    FileLockGuard(const FileLockGuard&) = delete;
    FileLockGuard& operator=(const FileLockGuard&) = delete;
private:
    std::string norm_path_;
};