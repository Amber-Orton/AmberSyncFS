#ifndef SYNC_H
#define SYNC_H

void upload_file(const char* path);
void delete_file(const char* path);
void opened_file(const char* path);

void upload_directory(const char* path);
void delete_directory(const char* path);
void opened_directory(const char* path);

#endif // SYNC_H