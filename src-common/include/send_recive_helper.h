
#ifndef SEND_RECIVE_HELPER_H
#define SEND_RECIVE_HELPER_H

#include <cstdint>
#include <string>

#include "send_recive.h"

// Writes data to the SSL connection untill all data is written or an error occurs. Returns number of bytes written or -1 on error.
int safe_SSL_write(Connection* conn, const void* buf, int num);

// Reads data from the SSL connection until all data is read or an error occurs. Returns number of bytes read or -1 on error.
int safe_SSL_read(Connection* conn, void* buf, int num);

// Returns the last modification time of the file in seconds since epoch.
uint64_t get_file_modification_time(const std::string& file_path);

// Sets the modification time of the file. Returns 0 on success, -1 on failure.
int set_file_modification_time(const std::string& file_path, uint64_t mod_time);

// Converts a 64-bit integer to network byte order.
uint64_t htonll(uint64_t value);

// Converts a 64-bit integer from network byte order to host byte order.
uint64_t ntohll(uint64_t value);

#endif