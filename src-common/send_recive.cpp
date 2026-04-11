#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <ostream>
#include "send_recive.h"
#include <chrono>
#include <mutex>
#include <algorithm>
#include <thread>
#include <condition_variable>
#include <filesystem>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


uint64_t get_file_modification_time(const std::string& file_path) {
    std::filesystem::path full_file_path(file_path);
    // check existance
    if (!std::filesystem::exists(full_file_path)) {
        return 0;
    }
    auto ftime = std::filesystem::last_write_time(full_file_path);
    auto standard_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now()
        + std::chrono::system_clock::now()
    );
    return std::chrono::system_clock::to_time_t(standard_time);
}

int set_file_modification_time(const std::string& file_path, uint64_t mod_time) {
    struct utimbuf new_times;
    new_times.actime = mod_time; // access time
    new_times.modtime = mod_time; // modification time
    if (utime(file_path.c_str(), &new_times) != 0) {
        std::cerr << "Failed to set file modification time for " << file_path << ": " << strerror(errno) << "\n";
        return -1;
    }
    return 0;
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


int send_file_tls(std::string relative_start_directory, std::string relative_file_path, Connection* conn) {
    // Open file
    std::string file_path = relative_start_directory + "/" + relative_file_path;
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file\n";
        return 1; // return 1 to indicate failure but do not retry as this is likely an issue with the file itself and not the connection
    }

    if (safe_SSL_write(conn, "UF", 2) < 0) { // Simple command to indicate upload
        std::cerr << "Failed to send upload command\n";
        return -1;
    }
    
    // Send file name length and name
    std::string filename = relative_file_path; // Send relative path for server dir structure
    uint32_t name_len = htonl(filename.size());
    if (safe_SSL_write(conn, &name_len, sizeof(name_len)) < 0) {
        std::cerr << "Failed to send file name length\n";
        return -1;
    }
    if (safe_SSL_write(conn, filename.c_str(), filename.size()) < 0) {
        std::cerr << "Failed to send file name\n";
        return -1;
    }

    std::time_t mod_time = get_file_modification_time(file_path);
    if (safe_SSL_write(conn, &mod_time, sizeof(mod_time)) < 0) {
        std::cerr << "Failed to send file modification time\n";
        return -1;
    }

    // Send file data
    char buffer[4096];
    while (file) {
        file.read(buffer, sizeof(buffer));
        std::streamsize bytes = file.gcount();
        if (bytes > 0) {
            if (safe_SSL_write(conn, buffer, bytes) < 0) {
                std::cerr << "Failed to send file data\n";
                return -1;
            }
        }
    }

    // Ensure file is flushed and closed before shutting down SSL
    file.close();
    return 0;
}

int send_delete_file_tls(std::string relative_file_path, uint64_t mod_time, Connection* conn) {
    if (safe_SSL_write(conn, "DF", 2) < 0) { // Simple command to indicate delete
        std::cerr << "Failed to send delete command\n";
        return -1;
    }

    // Send file name length and name
    std::string filename = relative_file_path; // Send relative path for server dir structure
    uint32_t name_len = htonl(filename.size());
    if (safe_SSL_write(conn, &name_len, sizeof(name_len)) < 0) {
        std::cerr << "Failed to send file name length\n";
        return -1;
    }
    if (safe_SSL_write(conn, filename.c_str(), filename.size()) < 0) {
        std::cerr << "Failed to send file name\n";
        return -1;
    }

    if (safe_SSL_write(conn, &mod_time, sizeof(mod_time)) < 0) {
        std::cerr << "Failed to send file modification time\n";
        return -1;
    }

    return 0;
}

int send_directory_tls(std::string relative_start_directory, std::string relative_directory_path, Connection* conn) {
    if (safe_SSL_write(conn, "UD", 2) < 0) { // Simple command to indicate upload directory
        std::cerr << "Failed to send upload directory command\n";
        return -1;
    }

    // Send directory name length and name
    std::string dirname = relative_directory_path; // Send relative path for server dir structure
    uint32_t name_len = htonl(dirname.size());
    if (safe_SSL_write(conn, &name_len, sizeof(name_len)) < 0) {
        std::cerr << "Failed to send directory name length\n";
        return -1;
    }
    if (safe_SSL_write(conn, dirname.c_str(), dirname.size()) < 0) {
        std::cerr << "Failed to send directory name\n";
        return -1;
    }

    // get and send directory modification time
    auto mod_time = get_file_modification_time(relative_start_directory + "/" + relative_directory_path);
    if (safe_SSL_write(conn, &mod_time, sizeof(mod_time)) < 0) {
        std::cerr << "Failed to send directory modification time\n";
        return -1;
    }

    return 0;
}

int send_delete_directory_tls(std::string relative_directory_path, uint64_t mod_time, Connection* conn) {
    if (safe_SSL_write(conn, "DD", 2) < 0) { // Simple command to indicate delete directory
        std::cerr << "Failed to send delete directory command\n";
        return -1;
    }

    // Send directory name length and name
    std::string dirname = relative_directory_path; // Send relative path for server dir structure
    uint32_t name_len = htonl(dirname.size());
    if (safe_SSL_write(conn, &name_len, sizeof(name_len)) < 0) {
        std::cerr << "Failed to send directory name length\n";
        return -1;
    }
    if (safe_SSL_write(conn, dirname.c_str(), dirname.size()) < 0) {
        std::cerr << "Failed to send directory name\n";
        return -1;
    }

    if (safe_SSL_write(conn, &mod_time, sizeof(mod_time)) < 0) {
        std::cerr << "Failed to send directory modification time\n";
        return -1;
    }

    return 0;
}

int receive_file_tls(std::string relative_directory_path, Connection* conn) {
    // read file name length and name
    uint32_t name_len = 0;
    int n1 = safe_SSL_read(conn, &name_len, sizeof(name_len));
    if (n1 <= 0) {
        std::cerr << "Failed to read file name length\n" << std::flush;
        return -1;
    }
    std::cout << "[DEBUG] Read file name length: " << n1 << " bytes\n" << std::flush;
    name_len = ntohl(name_len);
    if (name_len == 0 || name_len > 256) {
        std::cerr << "Invalid filename length: " << name_len << "\n" << std::flush;
        return -1;
    }
    
    std::string filename(name_len, '\0');
    int n2 = safe_SSL_read(conn, filename.data(), name_len);
    if (n2 <= 0) {
        std::cerr << "Failed to read file name\n" << std::flush;
        return -1;
    }
    std::cout << "[DEBUG] Read file name: '" << filename << "' (" << n2 << " bytes)\n" << std::flush;

    // read and check file modification time
    uint64_t mod_time = 0;
    if (safe_SSL_read(conn, &mod_time, sizeof(mod_time)) < 0) {
        std::cerr << "Failed to read file modification time\n" << std::flush;
        return -1;
    }
    auto current_mod_time = get_file_modification_time(relative_directory_path + "/" + filename);
    if (mod_time <= current_mod_time) {
        std::cerr << "Received file is not newer than existing file. Skipping update for '" << filename << "'\n" << std::flush;
        // Still need to read the incoming file data to clear the SSL buffer, but we can discard it since it's not newer
        char discard_buffer[4096];
        while (safe_SSL_read(conn, discard_buffer, sizeof(discard_buffer)) > 0) {
            // Discard data
        }
        return 1; // Not an error, just no update needed
    }

    std::filesystem::path output_path(relative_directory_path);
    output_path.append(filename);
    std::ofstream outfile(output_path, std::ios::binary);
    if (!outfile) {
        std::cerr << "Failed to open file for writing: " << filename << "\n" << std::flush;
        return -1;
    }
    std::cout << "Receiving file: '" << filename << "'..." << std::endl << std::flush;
    char buffer[4096];
    int bytes;
    size_t total_bytes = 0;
    while ((bytes = safe_SSL_read(conn, buffer, sizeof(buffer))) > 0) {
        if (bytes < 0) {
            std::cerr << "Error reading file data\n" << std::flush;
            outfile.close();
            return -1;
        }
        outfile.write(buffer, bytes);
        total_bytes += bytes;
    }
    std::cout << "Received file: '" << filename << "' (" << total_bytes << " bytes)" << std::endl << std::flush;
    
    outfile.flush();
    outfile.close();

    if (set_file_modification_time(output_path.string(), mod_time) < 0) {
        std::cerr << "Failed to set file modification time for '" << filename << " resetting to 0'\n" << std::flush;
        if (set_file_modification_time(output_path.string(), 0) < 0) {
            std::cerr << "Failed to reset file modification time for '" << filename << "'\n" << std::flush;
        }
        return -1;
    }
    return 0;
}

int receive_delete_file_tls(std::string relative_directory_path, Connection* conn) {
    uint32_t name_len = 0;
    int n1 = safe_SSL_read(conn, &name_len, sizeof(name_len));
    if (n1 <= 0) {
        std::cerr << "Failed to read file name length for deletion\n" << std::flush;
        return -1;
    }
    std::cout << "[DEBUG] Read file name length for deletion: " << n1 << " bytes\n" << std::flush;
    name_len = ntohl(name_len);
    if (name_len == 0 || name_len > 256) {
        std::cerr << "Invalid filename length for deletion: " << name_len << "\n" << std::flush;
        return -1;
    }

    std::string filename(name_len, '\0');
    int n2 = safe_SSL_read(conn, filename.data(), name_len);
    if (n2 <= 0) {
        std::cerr << "Failed to read file name for deletion\n" << std::flush;
        return -1;
    }
    std::cout << "[DEBUG] Read file name for deletion: '" << filename << "' (" << n2 << " bytes)\n" << std::flush;

    std::filesystem::path file_path(relative_directory_path);
    file_path.append(filename);
    if (!std::filesystem::exists(file_path)) {
        std::cerr << "File to delete does not exist: " << file_path << "\n" << std::flush;
        return -1;
    }

    // compare modification time
    uint64_t mod_time = 0;
    if (safe_SSL_read(conn, &mod_time, sizeof(mod_time)) < 0) {
        std::cerr << "Failed to read file modification time for deletion\n" << std::flush;
        return -1;
    }

    auto current_mod_time = get_file_modification_time(file_path.string());
    if (mod_time <= current_mod_time) {
        std::cerr << "Received delete command is not newer than existing file. Skipping deletion for '" << filename << "'\n" << std::flush;
        return 1; // Not an error, just no deletion needed
    }


    try {
        std::filesystem::remove(file_path);
        std::cout << "Deleted file: '" << file_path.string() << "'\n" << std::flush;
        return 0;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Failed to delete file: " << e.what() << "\n" << std::flush;
        return -1;
    }
}

int receive_directory_tls(std::string relative_directory_path, Connection* conn) {
    uint32_t name_len = 0;
    int n1 = safe_SSL_read(conn, &name_len, sizeof(name_len));
    if (n1 <= 0) {
        std::cerr << "Failed to read directory name length\n" << std::flush;
        return -1;
    }
    std::cout << "[DEBUG] Read directory name length: " << n1 << " bytes\n" << std::flush;
    name_len = ntohl(name_len);
    if (name_len == 0 || name_len > 256) {
        std::cerr << "Invalid directory name length: " << name_len << "\n" << std::flush;
        return -1;
    }

    std::string dirname(name_len, '\0');
    int n2 = safe_SSL_read(conn, dirname.data(), name_len);
    if (n2 <= 0) {
        std::cerr << "Failed to read directory name\n" << std::flush;
        return -1;
    }
    std::cout << "[DEBUG] Read directory name: '" << dirname << "' (" << n2 << " bytes)\n" << std::flush;

    // read and check directory modification time
    uint64_t mod_time = 0;
    if (safe_SSL_read(conn, &mod_time, sizeof(mod_time)) < 0) {
        std::cerr << "Failed to read directory modification time\n" << std::flush;
        return -1;
    }
    auto current_mod_time = get_file_modification_time(relative_directory_path + "/" + dirname);
    if (mod_time <= current_mod_time) {
        std::cerr << "Received directory is not newer than existing directory. Skipping update for '" << dirname << "'\n" << std::flush;
        return 1; // Not an error, just no update needed
    }

    if (std::filesystem::create_directories(relative_directory_path + "/" + dirname)) {
        std::cerr << "Failed to create directory: " << dirname << "\n" << std::flush;
        return -1;
	}

    return 0;
}

int receive_delete_directory_tls(std::string relative_directory_path, Connection* conn) {
        uint32_t name_len = 0;
    int n1 = safe_SSL_read(conn, &name_len, sizeof(name_len));
    if (n1 <= 0) {
        std::cerr << "Failed to read directory name length\n" << std::flush;
        return -1;
    }
    std::cout << "[DEBUG] Read directory name length: " << n1 << " bytes\n" << std::flush;
    name_len = ntohl(name_len);
    if (name_len == 0 || name_len > 256) {
        std::cerr << "Invalid directory name length: " << name_len << "\n" << std::flush;
        return -1;
    }

    std::string dirname(name_len, '\0');
    int n2 = safe_SSL_read(conn, dirname.data(), name_len);
    if (n2 <= 0) {
        std::cerr << "Failed to read directory name\n" << std::flush;
        return -1;
    }
    std::cout << "[DEBUG] Read directory name: '" << dirname << "' (" << n2 << " bytes)\n" << std::flush;

    // read and check directory modification time
    uint64_t mod_time = 0;
    if (safe_SSL_read(conn, &mod_time, sizeof(mod_time)) < 0) {
        std::cerr << "Failed to read directory modification time\n" << std::flush;
        return -1;
    }
    auto current_mod_time = get_file_modification_time(relative_directory_path + "/" + dirname);
    if (mod_time <= current_mod_time) {
        std::cerr << "Received delete command is not newer than existing directory. Skipping deletion for '" << dirname << "'\n" << std::flush;
        return 1; // Not an error, just no deletion needed
    }

    std::filesystem::remove_all(relative_directory_path + "/" + dirname);
    
    return 0;
}