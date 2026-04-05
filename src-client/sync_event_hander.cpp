#include "sync_event_handler.h"
#include "main.h"
#include <dirent.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include "connection.h"
#include <thread>


// hande the events created by the sync_event_creator for non priority events.
// This function continuously monitors the ready/non_priority directory for event files, processes them, and then deletes the event files.
// this does not use multithreading or async IO, so it will process events sequentially.
void handle_non_priority_events() {
    std::string ready_event_dir = event_dir + "/ready/non_priority";
    while (true) {
        DIR* dir = opendir(ready_event_dir.c_str());
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
                    printf("Found event file: %s\n", entry->d_name);
                    std::string event_file_path = ready_event_dir + "/" + entry->d_name;
                    
                    process_event_file(event_file_path);
                }
            }
            closedir(dir);
        } else {
            printf("Failed to open event directory: %s\n", ready_event_dir.c_str());
        }
        // sleep or yield to avoid busy-waiting
        usleep(100000); // 100ms
        // TODO: consider using inotify to watch for new files instead of polling
    }
}

// hande the events created by the sync_event_creator for priority events.
// This function continuously monitors the ready/priority directory for event files, processes them, and then deletes the event files.
// this does use multithreading or async IO, so it will process events in parallel.
void handle_priority_events() {
    std::string ready_event_dir = event_dir + "/ready/priority";
    while (true) {
        DIR* dir = opendir(ready_event_dir.c_str());
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
                    printf("Found event file: %s\n", entry->d_name);
                    std::string event_file_path = ready_event_dir + "/" + entry->d_name;
                    
                    // Move the event file to processing directory to indicate it's being processed
                    std::string processing_file_path = event_dir + "/processing/" + entry->d_name;
                    if (rename(event_file_path.c_str(), processing_file_path.c_str()) != 0) {
                        std::cerr << "Failed to move event file to processing: " << event_file_path << "\n";
                        continue;
                    }

                    // Process the event file in a new thread
                    std::thread event_thread(process_event_file, processing_file_path);
                    event_thread.detach(); // Detach the thread to allow it to run independently
                }
            }
            closedir(dir);
        } else {
            printf("Failed to open event directory: %s\n", ready_event_dir.c_str());
        }
        // sleep or yield to avoid busy-waiting
        usleep(10000); // 10ms
        // TODO: consider using inotify to watch for new files instead of polling
    }
}

void process_event_file(const std::string& event_file_path) {
    FILE* f = fopen(event_file_path.c_str(), "r");
    if (f) {
        char buf[256];
        fgets(buf, sizeof(buf), f);
        if (strcmp(buf, "UPLOAD_FILE\n") == 0) {
            if (fgets(buf, sizeof(buf), f)) {
                buf[strcspn(buf, "\n")] = 0; // Remove newline
                printf("Handling upload event for file: %s\n", buf);
                send_file_tls(buf);
            }
        } else {
            printf("Unknown event type in file %s: %s\n", event_file_path.c_str(), buf);
        }

        // close and delete the event file
        fclose(f);
        remove(event_file_path.c_str());
    } else {
        std::cerr << "Failed to open event file: " << event_file_path << "\n";
    }
}
    