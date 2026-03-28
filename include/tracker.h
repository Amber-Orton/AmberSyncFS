#include <filesystem>
#include <map>


int track_directory(const char* path);

void track_file(const std::filesystem::directory_entry& entry, int fd, std::map<int, std::string>& wd_to_path);