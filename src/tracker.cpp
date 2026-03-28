#ifndef TRACKER_H
#define TRACKER_H


#include <stdio.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <filesystem>



#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

int track_directory(const char* path) {
    int fd = inotify_init();

    inotify_add_watch(fd, path, IN_MODIFY | IN_CREATE);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
        if (entry.is_directory()) {
            inotify_add_watch(fd, entry.path().c_str(), IN_MODIFY | IN_CREATE);
        }
    }
    	
    char buffer[BUF_LEN];
    while (1) {
		int length = read(fd, buffer, BUF_LEN);

		for (int i = 0; i < length; ) {
			struct inotify_event *event = (struct inotify_event *)&buffer[i];

			if (event->len) {
				printf("Event on file: %s\n", event->name);
			}

			i += EVENT_SIZE + event->len;
		}
	}
    return 0;
}


#endif // TRACKER_H