#include <cstdio>
#include <atomic>
#include "sync_event_creator.h"
#include "connection.h"
#include "main.h"
#include <stdlib.h>
#include <iostream>
#include <unistd.h>

int create_priority_event_file(const char* event_type, const char* relative_path) {
    unsigned long event_num = event_counter.fetch_add(1, std::memory_order_relaxed);
    return create_event_file(event_type, "priority", relative_path, event_num);
}

int create_non_priority_event_file(const char* event_type, const char* relative_path) {
    unsigned long event_num = event_counter.fetch_add(1, std::memory_order_relaxed);
    return create_event_file(event_type, "non_priority", relative_path, event_num);
}

void upload_file(const char* relative_path) {
    printf("creating upload event file: %s\n", relative_path);

    if (create_non_priority_event_file("UPLOAD_FILE", relative_path) < 0) {
        std::cerr << "Failed to create upload event file\n";
        return;
    }
}

void delete_file(const char* relative_path) {
    printf("creating delete event file: %s\n", relative_path);

    if (create_non_priority_event_file("DELETE_FILE", relative_path) < 0) {
        std::cerr << "Failed to create delete event file\n";
        return;
    }
}

void opened_file(const char* relative_path) {
    printf("creating opened event file: %s\n", relative_path);

    if (create_priority_event_file("OPENED_FILE", relative_path) < 0) {
        std::cerr << "Failed to create opened event file\n";
        return;
    }
}

void upload_directory(const char* relative_path) {
    printf("creating upload directory event file: %s\n", relative_path);

    if (create_non_priority_event_file("UPLOAD_DIR", relative_path) < 0) {
        std::cerr << "Failed to create upload directory event file\n";
        return;
    }
}

void delete_directory(const char* relative_path) {
    printf("creating delete directory event file: %s\n", relative_path);

    if (create_non_priority_event_file("DELETE_DIR", relative_path) < 0) {
        std::cerr << "Failed to create delete directory event file\n";
        return;
    }
}

void opened_directory(const char* relative_path) {
    printf("creating opened directory event file: %s\n", relative_path);

    if (create_priority_event_file("OPENED_DIR", relative_path) < 0) {
        std::cerr << "Failed to create opened directory event file\n";
        return;
    }
}

int create_event_file(const char* event_type, const char* event_type_folder, const char* relative_path, unsigned long event_num) {
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
    snprintf(final_filename, sizeof(final_filename), "%s/ready/%s/event_%lu", event_dir, event_type_folder, event_num);
    rename(filename_template, final_filename);
    return 0;
}