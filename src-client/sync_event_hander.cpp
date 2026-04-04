#include "sync_event_handler.h"
#include "main.h"
#include <dirent.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include "connection.h"

void handle_events() {
    while (true) {
        char ready_event_dir[256];
        snprintf(ready_event_dir, sizeof(ready_event_dir), "%s/ready", event_dir);
        DIR* dir = opendir(ready_event_dir);
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
                    printf("Found event file: %s\n", entry->d_name);
                    char event_file_path[512];
                    snprintf(event_file_path, sizeof(event_file_path), "%s/%s", ready_event_dir, entry->d_name);
                    
                    FILE* f = fopen(event_file_path, "r");
                    if (f) {
                        char buf[256];
                        fgets(buf, sizeof(buf), f);
                        if (strcmp(buf, "UPLOAD\n") == 0) {
                            if (fgets(buf, sizeof(buf), f)) {
                                buf[strcspn(buf, "\n")] = 0; // Remove newline
                                printf("Handling upload event for file: %s\n", buf);
                                send_file_tls(buf);
                            }
                        } else {
                            printf("Unknown event type in file %s: %s\n", event_file_path, buf);
                        }

                        // close and delete the event file
                        fclose(f);
                        remove(event_file_path);
                    } else {
                        std::cerr << "Failed to open event file: " << event_file_path << "\n";
                    }
                }
            }
            closedir(dir);
        } else {
            std::cerr << "Failed to open event directory\n";
        }
        // sleep or yield to avoid busy-waiting
        usleep(100000); // 100ms
    }
}