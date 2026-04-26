#ifndef SYNC_H
#define SYNC_H
#include <cstdint>
#include <string>
#include "command.h"

// Creates an upload file event for the given path.
void upload_file(const std::string& relative_path);

// Creates a delete file event for the given path.
void delete_file(const std::string& relative_path);

// Creates an upload directory event for the given path.
void upload_directory(const std::string& relative_path);

// Creates a delete directory event for the given path.
void delete_directory(const std::string& relative_path);

// Checks if an event should be created. Returns 0 if created, 1 if not needed.
int create_event_checked(const CommandType& event_type, const std::string& relative_path, uint64_t mod_time);

#endif // SYNC_H