#include <stdio.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <filesystem>
#include "sync.h"
#include <map>
#include <string>


#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

void track_file(const std::filesystem::directory_entry& entry, int fd, std::map<int, std::string>& wd_to_path);

void track_directory(const char* path) {
    int fd = inotify_init();
    std::map<int, std::string> wd_to_path;

    track_file(std::filesystem::directory_entry(path), fd, wd_to_path);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
        if (entry.is_directory()) {
            track_file(entry, fd, wd_to_path);
        }
    }
    	
    char buffer[BUF_LEN];
    while (1) {
		int length = read(fd, buffer, BUF_LEN);

		for (int i = 0; i < length; ) {
			struct inotify_event *event = (struct inotify_event *)&buffer[i];

			if (event->len) {
                upload_file((wd_to_path[event->wd] + "/" + event->name).c_str());
			}

			i += EVENT_SIZE + event->len;
		}
	}
}

void track_file(const std::filesystem::directory_entry& entry, int fd, std::map<int, std::string>& wd_to_path) {
    int wd = inotify_add_watch(fd, entry.path().c_str(), IN_MODIFY | IN_CREATE);
    wd_to_path[wd] = entry.path().c_str();
}