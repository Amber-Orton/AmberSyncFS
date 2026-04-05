#include <cstdio>
#include <atomic>
#include "sync_event_creator.h"
#include "connection.h"
#include "timer_set.h"
#include "main.h"
#include <stdlib.h>
#include <iostream>
#include <unistd.h>

int create_priority_event_file(const std::string& event_type, const std::string& relative_path) {
    return create_event_file_checked(event_type, "priority", relative_path);
}

int create_non_priority_event_file(const std::string& event_type, const std::string& relative_path) {
    return create_event_file_checked(event_type, "non_priority", relative_path);
}

void upload_file(const std::string& relative_path) {
    printf("creating upload event file: %s\n", relative_path.c_str());
    
    if (create_non_priority_event_file("UPLOAD_FILE", relative_path) < 0) {
        std::cerr << "Failed to create upload event file\n";
        return;
    }
}

void delete_file(const std::string& relative_path) {
    printf("creating delete event file: %s\n", relative_path.c_str());
    
    if (create_non_priority_event_file("DELETE_FILE", relative_path) < 0) {
        std::cerr << "Failed to create delete event file\n";
        return;
    }
}

void opened_file(const std::string& relative_path) {
    printf("creating opened event file: %s\n", relative_path.c_str());
    
    if (create_priority_event_file("OPENED_FILE", relative_path) < 0) {
        std::cerr << "Failed to create opened event file\n";
        return;
    }
}

void upload_directory(const std::string& relative_path) {
    printf("creating upload directory event file: %s\n", relative_path.c_str());
    
    if (create_non_priority_event_file("UPLOAD_DIR", relative_path) < 0) {
        std::cerr << "Failed to create upload directory event file\n";
        return;
    }
}

void delete_directory(const std::string& relative_path) {
    printf("creating delete directory event file: %s\n", relative_path.c_str());
    
    if (create_non_priority_event_file("DELETE_DIR", relative_path) < 0) {
        std::cerr << "Failed to create delete directory event file\n";
        return;
    }
}

void opened_directory(const std::string& relative_path) {
    printf("creating opened directory event file: %s\n", relative_path.c_str());
    
    if (create_priority_event_file("OPENED_DIR", relative_path) < 0) {
        std::cerr << "Failed to create opened directory event file\n";
        return;
    }
}

int create_event_file(const std::string& event_type, const std::string& event_type_folder, const std::string& relative_path) {
    unsigned long event_num = event_counter.fetch_add(1, std::memory_order_relaxed);
    char filename_template[512];
    snprintf(filename_template, sizeof(filename_template), "%s/in_creation/event_XXXXXX", event_dir.c_str());
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
    snprintf(final_filename, sizeof(final_filename), "%s/ready/%s/event_%lu", event_dir.c_str(), event_type_folder.c_str(), event_num);
    rename(filename_template, final_filename);
    return 0;
}

int create_event_file_checked(const std::string& event_type, const std::string& event_type_folder, const std::string& relative_path) {
    if (timer_set_instance.check(event_type, event_type_folder, relative_path) == 0) {
        return -1;
    }
    return create_event_file(event_type, event_type_folder, relative_path);
}