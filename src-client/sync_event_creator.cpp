#include <cstdio>
#include <atomic>
#include "sync_event_creator.h"
#include "connection.h"
#include "timer_set.h"
#include "main.h"
#include <stdlib.h>
#include <iostream>
#include <unistd.h>
#include "../src-common/main.h"


void upload_file(const std::string& relative_path) {
    printf("creating upload event file: %s\n", relative_path.c_str());
    
    if (create_event_file_checked("UPLOAD_FILE", relative_path) < 0) {
        std::cerr << "Failed to create upload event file\n";
        return;
    }
}

void delete_file(const std::string& relative_path) {
    printf("creating delete event file: %s\n", relative_path.c_str());
    auto current_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
     
    if (create_event_file_checked("DELETE_FILE\n" + std::to_string(current_time), relative_path) < 0) {
        std::cerr << "Failed to create delete event file\n";
        return;
    }
}

void upload_directory(const std::string& relative_path) {
    printf("creating upload directory event file: %s\n", relative_path.c_str());
    
    if (create_event_file_checked("UPLOAD_DIR", relative_path) < 0) {
        std::cerr << "Failed to create upload directory event file\n";
        return;
    }
}

void delete_directory(const std::string& relative_path) {
    printf("creating delete directory event file: %s\n", relative_path.c_str());
    auto current_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    
    if (create_event_file_checked("DELETE_DIR\n" + std::to_string(current_time), relative_path) < 0) {
        std::cerr << "Failed to create delete directory event file\n";
        return;
    }
}

int create_event_file(const std::string& event_type, const std::string& relative_path) {
    unsigned long event_num = event_counter.fetch_add(1, std::memory_order_relaxed);
    char filename_template[512];
    snprintf(filename_template, sizeof(filename_template), "%s/in_creation/event_XXXXXX", data_dir.c_str());
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
    fprintf(event_file, "%s\n%s\n", event_type.c_str(), relative_path.c_str());
    fflush(event_file);
    fsync(fd);
    fclose(event_file);
    
    char final_filename[512];
    snprintf(final_filename, sizeof(final_filename), "%s/ready/event_%lu", data_dir.c_str(), event_num);
    rename(filename_template, final_filename);
    return 0;
}

int create_event_file_checked(const std::string& event_type, const std::string& relative_path) {
    if (timer_set_instance.check(event_type, relative_path) == 1) {
        std::cout << "Event creation skipped by timer set check for event type: " << event_type << ", relative path: " << relative_path << "\n";
        return 1; // Event creation not needed based on timer set check
    }
    return create_event_file(event_type, relative_path);
}