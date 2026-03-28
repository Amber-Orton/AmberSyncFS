#include <stdio.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <filesystem>
#include "sync.h"
#include <map>
#include <string>


#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))


void track_file_or_directory(const std::filesystem::directory_entry& entry, int fd, std::map<int, std::string>& wd_to_path);

void track_directory(const char* path) {
    int fd = inotify_init();
    std::map<int, std::string> wd_to_path;

    track_file_or_directory(std::filesystem::directory_entry(path), fd, wd_to_path);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
        if (entry.is_directory()) {
            track_file_or_directory(entry, fd, wd_to_path);
        }
    }
    	
    char buffer[BUF_LEN];
    while (1) {
        int length = read(fd, buffer, BUF_LEN);

        for (int i = 0; i < length; ) {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];

            if (event->len) {
                std::string full_path = wd_to_path[event->wd] + "/" + event->name;
                    bool is_dir = event->mask & IN_ISDIR;
                    if (event->mask & IN_MODIFY || event->mask & IN_CREATE) {
                        if (is_dir)
                            upload_directory(full_path.c_str());
                        else
                            upload_file(full_path.c_str());
                    }
                    if (event->mask & IN_DELETE || event->mask & IN_DELETE_SELF) {
                        if (is_dir)
                            delete_directory(full_path.c_str());
                        else
                            delete_file(full_path.c_str());
                    }
                    if (event->mask & IN_ACCESS || event->mask & IN_OPEN) {
                        if (is_dir)
                            opened_directory(full_path.c_str());
                        else
                            opened_file(full_path.c_str());
                    }
                    unsigned known = IN_MODIFY | IN_CREATE | IN_DELETE | IN_ACCESS | IN_OPEN | IN_DELETE_SELF | IN_ISDIR;
                    if ((event->mask & ~known) != 0) {
                        printf("Unknown/other event: %u\n", event->mask);
                    }
            }

            i += EVENT_SIZE + event->len;
        }
    }
}

void track_file_or_directory(const std::filesystem::directory_entry& entry, int fd, std::map<int, std::string>& wd_to_path) {
    int wd = inotify_add_watch(fd, entry.path().c_str(), IN_MODIFY | IN_CREATE | IN_DELETE | IN_ACCESS | IN_OPEN | IN_DELETE_SELF);
    wd_to_path[wd] = entry.path().c_str();
}