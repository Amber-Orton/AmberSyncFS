#ifndef SYNC_H
#define SYNC_H
#include <string>

void upload_file(const std::string& relative_path);
void delete_file(const std::string& relative_path);
void opened_file(const std::string& relative_path);

void upload_directory(const std::string& relative_path);
void delete_directory(const std::string& relative_path);
void opened_directory(const std::string& relative_path);

int create_priority_event_file(const std::string& event_type, const std::string& relative_path);
int create_non_priority_event_file(const std::string& event_type, const std::string& relative_path);
int create_event_file(const std::string& event_type, const std::string& event_type_folder, const std::string& relative_path);

#endif // SYNC_H