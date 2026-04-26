#include <cstdio>
#include <atomic>
#include "sync_event_creator.h"
#include "connection.h"
#include "timer_set.h"
#include "main.h"
#include <stdlib.h>
#include <iostream>
#include <unistd.h>
#include "send_recive_helper.h"
#include "database.h"


void upload_file(const std::string& relative_path) {
    printf("creating upload event file: %s\n", relative_path.c_str());
    auto mod_time = get_file_modification_time(relative_path);

    if (create_event_checked(CommandType::UPLOAD_FILE, relative_path, mod_time) < 0) {
        std::cerr << "Failed to create upload event file\n";
        return;
    }
}

void delete_file(const std::string& relative_path) {
    printf("creating delete event file: %s\n", relative_path.c_str());
    auto current_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
     
    if (create_event_checked(CommandType::DELETE_FILE, relative_path, current_time) < 0) {
        std::cerr << "Failed to create delete event file\n";
        return;
    }
}

void upload_directory(const std::string& relative_path) {
    printf("creating upload directory event file: %s\n", relative_path.c_str());
    auto mod_time = get_file_modification_time(relative_path);
    
    if (create_event_checked(CommandType::UPLOAD_DIRECTORY, relative_path, mod_time) < 0) {
        std::cerr << "Failed to create upload directory event file\n";
        return;
    }
}

void delete_directory(const std::string& relative_path) {
    printf("creating delete directory event file: %s\n", relative_path.c_str());
    auto current_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    
    if (create_event_checked(CommandType::DELETE_DIRECTORY, relative_path, current_time) < 0) {
        std::cerr << "Failed to create delete directory event file\n";
        return;
    }
}

int create_event_checked(const CommandType& event_type, const std::string& relative_path, uint64_t mod_time) {
    if (timer_set_instance.check(event_type, relative_path, mod_time) == 1) {
        std::cout << "Event creation skipped by timer set check for event type: " << event_type << ", relative path: " << relative_path << "\n";
        return 1; // Event creation not needed based on timer set check
    }
    create_event(event_type, relative_path, mod_time);

    events_cv.notify_one();
    return 0;
}