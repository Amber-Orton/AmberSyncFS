#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cerrno>
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <sys/types.h>
#include <utime.h>
#include <netinet/in.h>
#include <ctime>
#include <openssl/ssl.h>
#include "send_recive_helper.h"
#include "send_recive.h"
#include "connection.h"
#include "deleted_database.h"

uint64_t get_file_modification_time(const std::string& file_path) {
    std::filesystem::path full_file_path(file_path);
    // check existance
    if (!std::filesystem::exists(full_file_path)) {
        return get_delete_mtime(file_path); //if doesnt exist check db
    }
    auto ftime = std::filesystem::last_write_time(full_file_path);
    auto standard_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now()
        + std::chrono::system_clock::now()
    );
    return std::chrono::system_clock::to_time_t(standard_time);
}

int set_file_modification_time(const std::string& file_path, uint64_t mod_time) {
    if (std::filesystem::exists(file_path)) {
        struct utimbuf new_times;
        new_times.actime = mod_time; // access time
        new_times.modtime = mod_time; // modification time
        if (utime(file_path.c_str(), &new_times) != 0) {
            std::cerr << "Failed to set file modification time for " << file_path << ": " << strerror(errno) << "\n";
            return -1;
        }
        return 0;
    } else {
        set_delete_mtime(file_path, mod_time); // if doesnt exist set it as deleted
        return 0;
    }
}

int safe_SSL_write(Connection* conn, const void* buf, int num) {
    int total_written = 0;
    while (total_written < num) {
        int written = SSL_write(conn->ssl, (const char*)buf + total_written, num - total_written);
        if (written <= 0) {
            int err = SSL_get_error(conn->ssl, written);
            if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) {
                continue; // retry on these errors
            }
            std::cerr << "SSL_write failed with error: " << err << "\n";
            return -1; // indicate failure
        }
        total_written += written;
    }
    return total_written; // indicate success and return total bytes written
}

int safe_SSL_read(Connection* conn, void* buf, int num) {
    int total_read = 0;
    while (total_read < num) {
        int read_bytes = SSL_read(conn->ssl, (char*)buf + total_read, num - total_read);
        if (read_bytes <= 0) {
            int err = SSL_get_error(conn->ssl, read_bytes);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                continue; // retry on these errors
            }
            std::cerr << "safe_SSL_read failed with error: " << err << "\n";
            return -1; // indicate failure
        }
        total_read += read_bytes;
    }
    return total_read; // indicate success and return total bytes read
}
