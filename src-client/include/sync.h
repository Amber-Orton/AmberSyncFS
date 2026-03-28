#ifndef SYNC_H
#define SYNC_H

void upload_file(const char* track_root, const char* relative_path);
void delete_file(const char* track_root, const char* relative_path);
void opened_file(const char* track_root, const char* relative_path);

void upload_directory(const char* track_root, const char* relative_path);
void delete_directory(const char* track_root, const char* relative_path);
void opened_directory(const char* track_root, const char* relative_path);

#endif // SYNC_H