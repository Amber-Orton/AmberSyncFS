#include <filesystem>
#include <map>


void start_tracking_directory(const char* track_root);

void track_file_or_directory(const char* track_root, const std::filesystem::directory_entry& entry, int fd, std::map<int, std::string>& wd_to_path);