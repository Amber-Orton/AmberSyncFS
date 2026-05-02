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

#define FILE_COLOR "\033[1;33m" // Yellow for sync_event_creator.cpp
#define COLOR_RESET "\033[0m"


void upload_file(const std::string& relative_path) {
    std::cout << FILE_COLOR << "creating upload event file: " << relative_path << COLOR_RESET << "\n";
    auto mod_time = get_file_modification_time(relative_path);

    if (create_event_checked(CommandType::UPLOAD_FILE, relative_path, mod_time) < 0) {
        std::cerr << "Failed to create upload event file\n";
        return;
    }
}

void delete_file(const std::string& relative_path) {
    std::cout << FILE_COLOR << "creating delete event file: " << relative_path << COLOR_RESET << "\n";
    auto current_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
     
    if (create_event_checked(CommandType::DELETE_PATH, relative_path, current_time) < 0) {
        std::cerr << "Failed to create delete event file\n";
        return;
    }
}

void upload_directory(const std::string& relative_path) {
    std::cout << FILE_COLOR << "creating upload directory event file: " << relative_path << COLOR_RESET << "\n";
    auto mod_time = get_file_modification_time(relative_path);
    
    if (create_event_checked(CommandType::UPLOAD_DIRECTORY, relative_path, mod_time) < 0) {
        std::cerr << "Failed to create upload directory event file\n";
        return;
    }
}

void delete_directory(const std::string& relative_path) {
    std::cout << FILE_COLOR << "creating delete directory event file: " << relative_path << COLOR_RESET << "\n";
    auto current_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    
    if (create_event_checked(CommandType::DELETE_PATH, relative_path, current_time) < 0) {
        std::cerr << "Failed to create delete directory event file\n";
        return;
    }
}

int create_event_checked(const CommandType& event_type, const std::string& relative_path, uint64_t mod_time) {
    if (timer_set_instance.check(event_type, relative_path, mod_time) == 1) {
        std::cout << FILE_COLOR << "Event creation skipped by timer set check for event type: " << event_type << ", relative path: " << relative_path << COLOR_RESET << "\n";
        return 1; // Event creation not needed based on timer set check
    }
    create_event(event_type, relative_path, mod_time);

    events_cv.notify_one();
    return 0;
}