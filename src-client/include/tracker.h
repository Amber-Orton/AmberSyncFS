#include <filesystem>
#include <map>


void start_tracking();

void track_file_or_directory(const char* track_root, const std::filesystem::directory_entry& entry, int fd, std::map<int, std::string>& wd_to_path);