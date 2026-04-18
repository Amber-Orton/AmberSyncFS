#ifndef TRACKER_H
#define TRACKER_H

#include <filesystem>
#include <map>


void start_tracking();

void track_file_or_directory(const std::string& track_root, const std::filesystem::directory_entry& entry, int fd, std::map<int, std::string>& wd_to_path);

#endif