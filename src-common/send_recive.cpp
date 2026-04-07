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

int send_delete_file_tls(std::string relative_start_directory, std::string relative_file_path, Connection* conn) {
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

    return 0;
}

int send_delete_directory_tls(std::string relative_start_directory, std::string relative_directory_path, Connection* conn) {
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

    return 0;
}

int receive_file_tls(std::string directory, Connection* conn) {
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

    std::filesystem::path output_path(directory);
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
    return 0;
}

int receive_delete_file_tls(std::string directory, Connection* conn) {
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

    std::filesystem::path file_path(directory);
    file_path.append(filename);
    if (!std::filesystem::exists(file_path)) {
        std::cerr << "File to delete does not exist: " << file_path << "\n" << std::flush;
        return -1;
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

int receive_directory_tls(std::string directory, Connection* conn) {
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

    if (std::filesystem::create_directories(directory + "/" + dirname)) {
        std::cerr << "Failed to create directory: " << dirname << "\n" << std::flush;
        return -1;
	}

    return 0;
}

int receive_delete_directory_tls(std::string directory, Connection* conn) {
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

    std::filesystem::remove_all(directory + "/" + dirname);
    
    return 0;
}