#include "sync_event_handler.h"
#include "main.h"
#include <dirent.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include "connection.h"
#include <thread>
#include <semaphore>




// hande the events created by the sync_event_creator for priority events.
// This function continuously monitors the ready directory for event files, processes them, and then deletes the event files.
void handle_events() {
    std::string ready_event_dir = event_dir + "/ready";
    while (true) {
        DIR* dir = opendir(ready_event_dir.c_str());
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
                    std::string event_file_path = ready_event_dir + "/" + entry->d_name;
                    
                    // Move the event file to processing directory to indicate it's being processed
                    std::string processing_file_path = event_dir + "/processing/" + entry->d_name;
                    if (rename(event_file_path.c_str(), processing_file_path.c_str()) != 0) {
                        continue; // If we fail to move the file, skip processing it to avoid conflicts with other threads
                    }
                    printf("Found and processing event file: %s\n", entry->d_name);

                    if (process_event_file(processing_file_path) >= 0) {
                        remove(processing_file_path.c_str());
                    } else {
                        printf("Failed to process event file moving back to ready: %s\n", event_file_path.c_str());
                        // Move the event file back to the ready directory to retry later
                        if (rename(processing_file_path.c_str(), event_file_path.c_str()) != 0) {
                            printf("Failed to move event file back to ready: %s\npelase run a resync\n", processing_file_path.c_str());
                            remove(processing_file_path.c_str()); // If we fail to move it back, delete it to avoid blocking other events. This may cause missed events but is better than blocking.
                        }
                    }
                    // close and delete the event file
                }
            }
            closedir(dir);
        } else {
            printf("Failed to open event directory: %s\n", ready_event_dir.c_str());
        }
        // sleep or yield to avoid busy-waiting
        usleep(100000 * (1 + rand() % 5)); // ~100ms
        // TODO: consider using inotify to watch for new files instead of polling
        // or wait on on a mutex thats notified on event creation.
    }
}

int process_event_file(std::string event_file_path) {
    FILE* f = fopen(event_file_path.c_str(), "r");
    int sucess = -1;
    if (f) {
        char buf[256];
        fgets(buf, sizeof(buf), f);
        if (strcmp(buf, "UPLOAD_FILE\n") == 0) {
            if (fgets(buf, sizeof(buf), f)) {
                buf[strcspn(buf, "\n")] = 0; // Remove newline
                printf("Handling upload event for file: %s\n", buf);
                sucess = send_file(buf);
            }
        } else if (strcmp(buf, "DELETE_FILE\n") == 0) {
            if (fgets(buf, sizeof(buf), f)) {
                buf[strcspn(buf, "\n")] = 0; // Remove newline
                uint64_t mod_time = std::stoull(buf);
                if (fgets(buf, sizeof(buf), f)) {
                    buf[strcspn(buf, "\n")] = 0; // Remove newline
                    printf("Handling delete event for directory: %s, mod_time: %llu\n", buf, (unsigned long long)mod_time);
                    sucess = send_delete_file(buf, mod_time);
                }
            }
        } else if (strcmp(buf, "UPLOAD_DIR\n") == 0) {
            if (fgets(buf, sizeof(buf), f)) {
                buf[strcspn(buf, "\n")] = 0; // Remove newline
                printf("Handling upload event for directory: %s\n", buf);
                sucess = send_directory(buf);
            }
        } else if (strcmp(buf, "DELETE_DIR\n") == 0) {
            if (fgets(buf, sizeof(buf), f)) {
                buf[strcspn(buf, "\n")] = 0; // Remove newline
                uint64_t mod_time = std::stoull(buf);
                if (fgets(buf, sizeof(buf), f)) {
                    buf[strcspn(buf, "\n")] = 0; // Remove newline
                    printf("Handling delete event for directory: %s, mod_time: %llu\n", buf, (unsigned long long)mod_time);
                    sucess = send_delete_directory(buf, mod_time);
                }
            }
        } else {
            printf("Unknown event type in file %s: %s\n", event_file_path.c_str(), buf);
        }
        
        fclose(f);
    } else {
        std::cerr << "Failed to open event file: " << event_file_path << "\n";
    }
    return sucess;
}
    