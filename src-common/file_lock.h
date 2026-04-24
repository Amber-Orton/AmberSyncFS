
#include <string>
#include <filesystem>

void lock_file(const std::string& path);
void unlock_file(const std::string& path);

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