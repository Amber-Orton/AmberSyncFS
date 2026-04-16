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
#include "send_recive_helper.h"

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

    uint64_t file_size = std::filesystem::file_size(file_path);
    if (safe_SSL_write(conn, &file_size, sizeof(file_size)) < 0) {
        std::cerr << "Failed to send file size\n";
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

int receive_file_tls(std::string relative_directory_path, Connection* conn, Command* out_command) {
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

    // read file size
    uint64_t bits_left = 0;
    if (safe_SSL_read(conn, &bits_left, sizeof(bits_left)) < 0) {
        std::cerr << "Failed to read file size\n" << std::flush;
        return -1;
    }

    // Open file for writing
    std::filesystem::path output_path(relative_directory_path);
    output_path.append(filename);

    // update out_command
    if (out_command) {
        out_command->path = filename;
    }

    auto old_exists = std::filesystem::exists(output_path);
    if (old_exists) {
        if (rename(output_path.string().c_str(), (output_path.string() + ".tmp").c_str()) < 0) {
            if (errno != ENOENT) { // If the file doesn't exist, that's fine, we will create it. But if rename fails for another reason, we should report it.
                std::cerr << "Failed to rename existing file to temporary name for safe writing: " << strerror(errno) << "\n" << std::flush;
                return -1;
            }
        }
    }

    std::ofstream outfile(output_path, std::ios::binary);
    if (!outfile) {
        std::cerr << "Failed to open file for writing: " << filename << "\n" << std::flush;
        if (old_exists) {
            // Try to restore old file from temporary name if we failed to open new file for writing
            rename((output_path.string() + ".tmp").c_str(), output_path.string().c_str());
        }
        return -1;
    }

    // Read file data
    std::cout << "Receiving file: '" << filename << "'..." << std::endl << std::flush;
    char buffer[4096]; // Use smaller buffer if file is smaller than 4KB
    int bytes;
    size_t total_bytes = 0;
    int bits_to_read = (bits_left < sizeof(buffer)) ? bits_left : sizeof(buffer);
    while ((bytes = safe_SSL_read(conn, buffer, bits_to_read)) > 0) {
        if (bytes < 0) {
            std::cerr << "Error reading file data\n" << std::flush;
            outfile.close();
            std::filesystem::remove(output_path); // Remove incomplete file
            if (old_exists) {
                rename((output_path.string() + ".tmp").c_str(), output_path.string().c_str());
            }
            return -1;
        }
        outfile.write(buffer, bytes);
        total_bytes += bytes;
        bits_left -= bytes;
        if (bits_left == 0) {
            break; // Finished reading file
        }
        bits_to_read = (bits_left < sizeof(buffer)) ? bits_left : sizeof(buffer);
    }
    std::cout << "Received file: '" << filename << "' (" << total_bytes << " bytes)" << std::endl << std::flush;
    
    outfile.flush();
    outfile.close();

    if (set_file_modification_time(output_path.string(), mod_time) < 0) {
        std::cerr << "Failed to set file modification time for '" << filename << " resetting to 0'\n" << std::flush;
        if (set_file_modification_time(output_path.string(), 0) < 0) {
            std::cerr << "Failed to reset file modification time for '" << filename << "'\n" << std::flush;
        }
        if (old_exists) {
            rename((output_path.string() + ".tmp").c_str(), output_path.string().c_str());
        }
        return -1;
    }
    return 0;
}

int receive_delete_file_tls(std::string relative_directory_path, Connection* conn, Command* out_command) {
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

    // update out_command
    if (out_command) {
        out_command->path = filename;
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

int receive_directory_tls(std::string relative_directory_path, Connection* conn, Command* out_command) {
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

    // update out_command
    if (out_command) {
        out_command->path = dirname;
    }

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

int receive_delete_directory_tls(std::string relative_directory_path, Connection* conn, Command* out_command) {
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

    // update out_command
    if (out_command) {
        out_command->path = dirname;
    }

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

int handle_incoming_command(Connection* conn, std::string relative_start_directory, Command* out_command) {
    // Read command from client
    char command[2];
    int bytes_read = safe_SSL_read(conn, command, sizeof(command)); // Read command type (e.g., "UP" for upload)
    if (bytes_read <= 0) {
        std::cerr << "Failed to read command from client\n";
        return -1;
    }
    out_command->type = std::string(command, 2);


    // Handle command, its the programmers responsibility to ensure that commands are all distinct and of a fixed length
    // each in a thread, allows infinite clients to connect and send commands without blocking each other
    // means can be DOSed by opening many connections and sending commands without closing them but clients are certified and assumed to be non-malicious
    if (std::strncmp(command, "UF", 2) == 0) {
        std::cout << "Received upload command from client\n";
        if (receive_file_tls(relative_start_directory, conn, out_command) < 0) {
            std::cerr << "Failed to receive file\n";
            return -1;
        } else {
            return 0;
        }
    } else if (std::strncmp(command, "DF", 2) == 0) {
        std::cout << "Received delete command from client\n";
        if (receive_delete_file_tls(relative_start_directory, conn, out_command) < 0) {
            std::cerr << "Failed to delete file\n";
            return -1;
        } else {
            return 0;
        }
    } else if (std::strncmp(command, "UD", 2) == 0) {
        std::cout << "Received upload directory command from client\n";
        if (receive_directory_tls(relative_start_directory, conn, out_command) < 0) {
            std::cerr << "Failed to receive directory\n";
            return -1;
        } else {
            return 0;
        }
    } else if (std::strncmp(command, "DD", 2) == 0) {
        std::cout << "Received delete directory command from client\n";
        if (receive_delete_directory_tls(relative_start_directory, conn, out_command) < 0) {
            std::cerr << "Failed to delete directory\n";
            return -1;
        } else {
            return 0;
        }
    } else {
        std::cerr << "Unknown command received from client: " << std::string(command, 2) << "\n";
        return -1;
    }
}