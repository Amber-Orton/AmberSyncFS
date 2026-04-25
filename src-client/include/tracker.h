#ifndef TRACKER_H
#define TRACKER_H

#include <filesystem>
#include <map>



// Starts the file/directory tracking process.
void start_tracking();

// Tracks a file or directory and updates the watch descriptor map.
void track_file_or_directory(const std::string& track_root, const std::filesystem::directory_entry& entry, int fd, std::map<int, std::string>& wd_to_path);

#endif