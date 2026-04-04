#include <cstdio>
#include <atomic>
#include "sync_event_creator.h"
#include "connection.h"
#include "main.h"
#include <stdlib.h>
#include <iostream>
#include <unistd.h>

void upload_file(const char* relative_path) {
    printf("creating upload event file: %s\n", relative_path);
    
    unsigned long event_num = event_counter.fetch_add(1, std::memory_order_relaxed);
    if (create_event_file("UPLOAD", relative_path, event_num) < 0) {
        std::cerr << "Failed to create upload event file\n";
        return;
    }
}

void delete_file(const char* relative_path) {
    // Placeholder for file deletion logic
    // printf("Deleting file: %s\n", track_root, relative_path);
}

void opened_file(const char* relative_path) {
    // Placeholder for file opened logic
    // printf("File opened: %s\n", track_root, relative_path);
}

void upload_directory(const char* relative_path) {
    // Placeholder for directory upload logic
    // printf("Uploading directory: %s\n", track_root, relative_path);
}

void delete_directory(const char* relative_path) {
    // Placeholder for directory deletion logic
    // printf("Deleting directory: %s\n", track_root, relative_path);
}

void opened_directory(const char* relative_path) {
    // Placeholder for directory opened logic
    // printf("Directory opened: %s\n", track_root, relative_path);
}

int create_event_file(const char* event_type, const char* relative_path, unsigned long event_num) {
    char filename_template[64];
    snprintf(filename_template, sizeof(filename_template), "%s/in_creation/event_XXXXXX", event_dir);
    int fd = mkstemp(filename_template);
    if (fd < 0) {
        std::cerr << "Failed to create temp event file\n";
        return -1;
    }
    FILE* event_file = fdopen(fd, "w");
    if (!event_file) {
        std::cerr << "Failed to open event file stream\n";
        close(fd);
        return -1;
    }
    fprintf(event_file, "%s\n%s\n", event_type, relative_path);
    fflush(event_file);
    fsync(fd);
    fclose(event_file);

    char final_filename[64];
    snprintf(final_filename, sizeof(final_filename), "%s/ready/event_%lu", event_dir, event_num);
    rename(filename_template, final_filename);
    return 0;
}