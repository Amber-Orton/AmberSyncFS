#ifndef SYNC_H
#define SYNC_H

void upload_file(const char* relative_path);
void delete_file(const char* relative_path);
void opened_file(const char* relative_path);

void upload_directory(const char* relative_path);
void delete_directory(const char* relative_path);
void opened_directory(const char* relative_path);

int create_event_file(const char* event_type, const char* event_type_folder, const char* relative_path, unsigned long event_num);

#endif // SYNC_H