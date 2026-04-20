#ifndef SYNC_H
#define SYNC_H
#include <cstdint>
#include <string>

void upload_file(const std::string& relative_path);
void delete_file(const std::string& relative_path);

void upload_directory(const std::string& relative_path);
void delete_directory(const std::string& relative_path);

int create_event_checked(const std::string& event_type, const std::string& relative_path, uint64_t mod_time);

#endif // SYNC_H