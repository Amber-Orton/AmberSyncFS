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
#include "database.h"
#include "file_lock.h"
#include "command.h"


// functions to handle sending and reciving events
// when interacting with files or directories make sure to use the FileLockGuard to avaoid race conditions
// use safe_SSL_write and safe_SSL_read for all SSL interactions to properly handle errors and disconnections
// connections should be established using the try_establish_connection
// and ending the connection is down to the initialiser of the connection


// connection follows this pattern for every event:
// (each step first send size of data for this step as uint64_t in network byte order, then send the data itself)
// Client: sends their name
// Client: sends command (no length needed since all commands are same length and can be read in one go)
// where needed Client: sends file/directory name
// where needed Client: sends file modification time as uint64_t in network byte order
// where needed Client: sends file data
// Server: sends number of pending events for the client as uint64_t in network byte order (no length needed since it's always the same size)

int send_file_tls(std::string relative_start_directory, std::string relative_file_path, Connection* conn) {
    
    std::filesystem::path full_path(relative_start_directory);
    full_path.append(relative_file_path);
    FileLockGuard guard(full_path.lexically_normal().string()); // Lock normalized file path

    // Open file
    std::ifstream file(full_path, std::ios::binary);
    if (!file) {
        std::cerr << "[send_recive.cpp:send_file_tls] Failed to open file, just requesting number of pending events\n";
        CommandType command = CommandType::REQUEST_NUMBER_PENDING_EVENTS; // Send a command to indicate failure but still close connection gracefully
        if (safe_SSL_write(conn, &command, sizeof(command)) < 0) { // Send a command to indicate failure but still close connection gracefully
            std::cerr << "[send_recive.cpp:send_file_tls] Failed to send request number of pending events command\n";
            return -1;
        }
        return 1; // return 1 to indicate failure but do not retry as this is likely an issue with the file itself and not the connection
    }

    CommandType command = CommandType::UPLOAD_FILE;
    if (safe_SSL_write(conn, &command, sizeof(command)) < 0) { // Simple command to indicate upload
        std::cerr << "[send_recive.cpp:send_file_tls] Failed to send upload command\n";
        return -1;
    }
    
    // Send file name length and name
    std::string filename = relative_file_path; // Send relative path for server dir structure
    uint64_t name_len = htonl(filename.size());
    if (safe_SSL_write(conn, &name_len, sizeof(name_len)) < 0) {
        std::cerr << "[send_recive.cpp:send_file_tls] Failed to send file name length\n";
        return -1;
    }
    if (safe_SSL_write(conn, filename.c_str(), filename.size()) < 0) {
        std::cerr << "[send_recive.cpp:send_file_tls] Failed to send file name\n";
        return -1;
    }

    uint64_t mod_time = get_file_modification_time(full_path);
    uint64_t mod_time_net = htonll(mod_time);
    if (safe_SSL_write(conn, &mod_time_net, sizeof(mod_time_net)) < 0) {
        std::cerr << "[send_recive.cpp:send_file_tls] Failed to send file modification time\n";
        return -1;
    }

    uint64_t file_size = std::filesystem::file_size(full_path);
    uint64_t file_size_net = htonll(file_size);
    if (safe_SSL_write(conn, &file_size_net, sizeof(file_size_net)) < 0) {
        std::cerr << "[send_recive.cpp:send_file_tls] Failed to send file size\n";
        return -1;
    }

    // Send file data
    char buffer[4096];
    while (file) {
        file.read(buffer, sizeof(buffer));
        std::streamsize bytes = file.gcount();
        if (bytes > 0) {
            if (safe_SSL_write(conn, buffer, bytes) < 0) {
                std::cerr << "[send_recive.cpp:send_file_tls] Failed to send file data\n";
                return -1;
            }
        }
    }

    // Ensure file is flushed and closed before shutting down SSL
    file.close();
    return 0;
}

int send_delete_file_tls(std::string relative_start_directory, std::string relative_file_path, uint64_t mod_time, Connection* conn) {
    std::filesystem::path full_path(relative_start_directory);
    full_path.append(relative_file_path);
    FileLockGuard guard(full_path.lexically_normal().string()); // Lock normalized file path

    CommandType command = CommandType::DELETE_FILE;
    if (safe_SSL_write(conn, &command, sizeof(command)) < 0) {
        std::cerr << "[send_recive.cpp:send_delete_file_tls] Failed to send delete command\n";
        return -1;
    }

    // Send file name length and name
    std::string filename = relative_file_path; // Send relative path for server dir structure
    uint64_t name_len = htonl(filename.size());
    if (safe_SSL_write(conn, &name_len, sizeof(name_len)) < 0) {
        std::cerr << "[send_recive.cpp:send_delete_file_tls] Failed to send file name length\n";
        return -1;
    }
    if (safe_SSL_write(conn, filename.c_str(), filename.size()) < 0) {
        std::cerr << "[send_recive.cpp:send_delete_file_tls] Failed to send file name\n";
        return -1;
    }

    uint64_t mod_time_net = htonll(mod_time);
    if (safe_SSL_write(conn, &mod_time_net, sizeof(mod_time_net)) < 0) {
        std::cerr << "[send_recive.cpp:send_delete_file_tls] Failed to send file modification time\n";
        return -1;
    }

    return 0;
}

int send_directory_tls(std::string relative_start_directory, std::string relative_directory_path, Connection* conn) {
    std::filesystem::path full_path(relative_start_directory);
    full_path.append(relative_directory_path);
    FileLockGuard guard(full_path.lexically_normal().string()); // Lock normalized directory path
    
    CommandType command = CommandType::UPLOAD_DIRECTORY;
    if (safe_SSL_write(conn, &command, sizeof(command)) < 0) { // Simple command to indicate upload directory
        std::cerr << "[send_recive.cpp:send_directory_tls] Failed to send upload directory command\n";
        return -1;
    }

    // Send directory name length and name
    std::string dirname = relative_directory_path; // Send relative path for server dir structure
    uint64_t name_len = htonl(dirname.size());
    if (safe_SSL_write(conn, &name_len, sizeof(name_len)) < 0) {
        std::cerr << "[send_recive.cpp:send_directory_tls] Failed to send directory name length\n";
        return -1;
    }
    if (safe_SSL_write(conn, dirname.c_str(), dirname.size()) < 0) {
        std::cerr << "[send_recive.cpp:send_directory_tls] Failed to send directory name\n";
        return -1;
    }

    // get and send directory modification time
    auto mod_time = get_file_modification_time(full_path.string());
    uint64_t mod_time_net = htonll(mod_time);
    if (safe_SSL_write(conn, &mod_time_net, sizeof(mod_time_net)) < 0) {
        std::cerr << "[send_recive.cpp:send_directory_tls] Failed to send directory modification time\n";
        return -1;
    }

    return 0;
}

int send_delete_directory_tls(std::string relative_start_directory, std::string relative_directory_path, uint64_t mod_time, Connection* conn) {
    std::filesystem::path full_path(relative_start_directory);
    full_path.append(relative_directory_path);
    FileLockGuard guard(full_path.lexically_normal().string()); // Lock normalized directory path

    CommandType command = CommandType::DELETE_DIRECTORY;
    if (safe_SSL_write(conn, &command, sizeof(command)) < 0) { // Simple command to indicate delete directory
        std::cerr << "[send_recive.cpp:send_delete_directory_tls] Failed to send delete directory command\n";
        return -1;
    }

    // Send directory name length and name
    std::string dirname = relative_directory_path; // Send relative path for server dir structure
    uint64_t name_len = htonl(dirname.size());
    if (safe_SSL_write(conn, &name_len, sizeof(name_len)) < 0) {
        std::cerr << "[send_recive.cpp:send_delete_directory_tls] Failed to send directory name length\n";
        return -1;
    }
    if (safe_SSL_write(conn, dirname.c_str(), dirname.size()) < 0) {
        std::cerr << "[send_recive.cpp:send_delete_directory_tls] Failed to send directory name\n";
        return -1;
    }

    uint64_t mod_time_net = htonll(mod_time);
    if (safe_SSL_write(conn, &mod_time_net, sizeof(mod_time_net)) < 0) {
        std::cerr << "[send_recive.cpp:send_delete_directory_tls] Failed to send directory modification time\n";
        return -1;
    }

    return 0;
}

int receive_file_tls(std::string relative_start_directory, Connection* conn, Event* out_event) {
    // read file name length and name
    uint64_t name_len = 0;
    int n1 = safe_SSL_read(conn, &name_len, sizeof(name_len));
    if (n1 <= 0) {
        std::cerr << "[send_recive.cpp:receive_file_tls] Failed to read file name length\n" << std::flush;
        return -1;
    }
    std::cout << "[send_recive.cpp:receive_file_tls] Read file name length: " << n1 << " bytes\n" << std::flush;
    name_len = ntohl(name_len);
    if (name_len == 0 || name_len > 256) {
        std::cerr << "[send_recive.cpp:receive_file_tls] Invalid filename length: " << name_len << "\n" << std::flush;
        return -1;
    }
    
    std::string filename(name_len, '\0');
    int n2 = safe_SSL_read(conn, filename.data(), name_len);
    if (n2 <= 0) {
        std::cerr << "[send_recive.cpp:receive_file_tls] Failed to read file name\n" << std::flush;
        return -1;
    }
    std::cout << "[send_recive.cpp:receive_file_tls] Read file name: '" << filename << "' (" << n2 << " bytes)\n" << std::flush;

    std::filesystem::path output_path(relative_start_directory);
    output_path.append(filename);
    FileLockGuard guard(output_path.lexically_normal().string()); // Lock normalized file path

    // update out_event
    if (out_event) {
        out_event->path = filename;
    }

    // read file modification time
    uint64_t mod_time_net = 0;
    if (safe_SSL_read(conn, &mod_time_net, sizeof(mod_time_net)) < 0) {
        std::cerr << "[send_recive.cpp:receive_file_tls] Failed to read file modification time\n" << std::flush;
        return -1;
    }
    uint64_t mod_time = ntohll(mod_time_net);

    // update out_event
    if (out_event) {
        out_event->timestamp = mod_time;
    }

    
    // read file size
    uint64_t bits_left_net = 0;
    if (safe_SSL_read(conn, &bits_left_net, sizeof(bits_left_net)) < 0) {
        std::cerr << "[send_recive.cpp:receive_file_tls] Failed to read file size\n" << std::flush;
        return -1;
    }
    uint64_t bits_left = ntohll(bits_left_net);
    
    auto current_mod_time = get_file_modification_time(relative_start_directory + "/" + filename);
    if (mod_time <= current_mod_time) {
        std::cerr << "[send_recive.cpp:receive_file_tls] Received file is not newer than existing file. Skipping update for '" << filename << "'\n" << std::flush;
        // Still need to read the incoming file data to clear the SSL buffer, but we can discard it since it's not newer
        char buffer[4096];
        while (bits_left > 0) {
            int bits_to_read = (bits_left < sizeof(buffer)) ? bits_left : sizeof(buffer);
            int n = safe_SSL_read(conn, buffer, bits_to_read);
            if (n <= 0) {
                std::cerr << "[send_recive.cpp:receive_file_tls] Failed to read file data for skipped update\n" << std::flush;
                return -1;
            }
            bits_left -= n;
        }
        return 1; // Not an error, just no update needed
    }

    // Open file for writing move old file to temp to avaoid issues with lost connections
    auto old_exists = std::filesystem::exists(output_path);
    if (old_exists) {
        if (rename(output_path.string().c_str(), (output_path.string() + ".tmp").c_str()) < 0) {
            if (errno != ENOENT) { // If the file doesn't exist, that's fine, we will create it. But if rename fails for another reason, we should report it.
                std::cerr << "[send_recive.cpp:receive_file_tls] Failed to rename existing file to temporary name for safe writing: " << strerror(errno) << "\n" << std::flush;
                return -1;
            }
        }
    }

    std::ofstream outfile(output_path, std::ios::binary);
    if (!outfile) {
        std::cerr << "[send_recive.cpp:receive_file_tls] Failed to open file for writing: " << filename << "\n" << std::flush;
        if (old_exists) {
            // Try to restore old file from temporary name if we failed to open new file for writing
            rename((output_path.string() + ".tmp").c_str(), output_path.string().c_str());
        }
        return -1;
    }

    // Read file data
    std::cout << "[send_recive.cpp:receive_file_tls] Receiving file: '" << filename << "'..." << std::endl << std::flush;
    char buffer[4096]; // Use smaller buffer if file is smaller than 4KB
    int bytes;
    size_t total_bytes = 0;
    int bits_to_read = (bits_left < sizeof(buffer)) ? bits_left : sizeof(buffer);
    while ((bytes = safe_SSL_read(conn, buffer, bits_to_read)) > 0) {
        if (bytes < 0) {
            std::cerr << "[send_recive.cpp:receive_file_tls] Error reading file data\n" << std::flush;
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
    std::cout << "[send_recive.cpp:receive_file_tls] Received file: '" << filename << "' (" << total_bytes << " bytes)" << std::endl << std::flush;
    
    outfile.flush();
    outfile.close();

    if (set_file_modification_time(output_path.string(), mod_time) < 0) {
        std::cerr << "[send_recive.cpp:receive_file_tls] Failed to set file modification time for '" << filename << " resetting to 0'\n" << std::flush;
        if (set_file_modification_time(output_path.string(), 0) < 0) {
            std::cerr << "[send_recive.cpp:receive_file_tls] Failed to reset file modification time for '" << filename << "'\n" << std::flush;
        }
        if (old_exists) {
            rename((output_path.string() + ".tmp").c_str(), output_path.string().c_str());
        }
        return -1;
    }
    // everything worked remove temp file if it exists
    if (old_exists) {
        printf("[send_recive.cpp:receive_file_tls] Removing temporary file: %s\n", (output_path.string() + ".tmp").c_str());
        std::filesystem::remove(output_path.string() + ".tmp");
    }
    return 0;
}

int receive_delete_file_tls(std::string relative_start_directory, Connection* conn, Event* out_event) {
    uint64_t name_len = 0;
    int n1 = safe_SSL_read(conn, &name_len, sizeof(name_len));
    if (n1 <= 0) {
        std::cerr << "[send_recive.cpp:receive_delete_file_tls] Failed to read file name length for deletion\n" << std::flush;
        return -1;
    }
    std::cout << "[send_recive.cpp:receive_delete_file_tls] Read file name length for deletion: " << n1 << " bytes\n" << std::flush;
    name_len = ntohl(name_len);
    if (name_len == 0 || name_len > 256) {
        std::cerr << "[send_recive.cpp:receive_delete_file_tls] Invalid filename length for deletion: " << name_len << "\n" << std::flush;
        return -1;
    }

    std::string filename(name_len, '\0');
    int n2 = safe_SSL_read(conn, filename.data(), name_len);
    if (n2 <= 0) {
        std::cerr << "[send_recive.cpp:receive_delete_file_tls] Failed to read file name for deletion\n" << std::flush;
        return -1;
    }
    std::cout << "[send_recive.cpp:receive_delete_file_tls] Read file name for deletion: '" << filename << "' (" << n2 << " bytes)\n" << std::flush;

    std::filesystem::path file_path(relative_start_directory);
    file_path.append(filename);
    FileLockGuard guard(file_path.lexically_normal().string()); // Lock normalized file path


    // update out_event
    if (out_event) {
        out_event->path = filename;
    }

    // compare modification time
    uint64_t mod_time_net = 0;
    if (safe_SSL_read(conn, &mod_time_net, sizeof(mod_time_net)) < 0) {
        std::cerr << "[send_recive.cpp:receive_delete_file_tls] Failed to read file modification time for deletion\n" << std::flush;
        return -1;
    }
    uint64_t mod_time = ntohll(mod_time_net);

    // update out_event
    if (out_event) {
        out_event->timestamp = mod_time;
    }

    auto current_mod_time = get_file_modification_time(file_path.string());
    if (mod_time <= current_mod_time) {
        std::cerr << "[send_recive.cpp:receive_delete_file_tls] Received delete command is not newer than existing file. Skipping deletion for '" << filename << "'\n" << std::flush;
        return 1; // Not an error, just no deletion needed
    }

    std::error_code ec;
    bool removed = std::filesystem::remove(file_path, ec);
    if (ec) {
        std::cerr << "[send_recive.cpp:receive_delete_file_tls] Failed to delete file: " << ec.message() << "\n" << std::flush;
        return -1;
    }

    if (removed) {
        std::cout << "[send_recive.cpp:receive_delete_file_tls] Deleted file: '" << file_path.string() << "'\n" << std::flush;
    } else {
        std::cout << "[send_recive.cpp:receive_delete_file_tls] Delete target already absent, recording delete time: '" << file_path.string() << "'\n" << std::flush;
    }

    // Record deletion time so stale uploads can be rejected later.
    set_delete_mtime(file_path.string(), mod_time);
    return 0;
}

int receive_directory_tls(std::string relative_start_directory, Connection* conn, Event* out_event) {
    uint64_t name_len = 0;
    int n1 = safe_SSL_read(conn, &name_len, sizeof(name_len));
    if (n1 <= 0) {
        std::cerr << "[send_recive.cpp:receive_directory_tls] Failed to read directory name length\n" << std::flush;
        return -1;
    }
    std::cout << "[send_recive.cpp:receive_directory_tls] Read directory name length: " << n1 << " bytes\n" << std::flush;
    name_len = ntohl(name_len);
    if (name_len == 0 || name_len > 256) {
        std::cerr << "[send_recive.cpp:receive_directory_tls] Invalid directory name length: " << name_len << "\n" << std::flush;
        return -1;
    }

    std::string dirname(name_len, '\0');
    int n2 = safe_SSL_read(conn, dirname.data(), name_len);
    if (n2 <= 0) {
        std::cerr << "[send_recive.cpp:receive_directory_tls] Failed to read directory name\n" << std::flush;
        return -1;
    }
    std::cout << "[send_recive.cpp:receive_directory_tls] Read directory name: '" << dirname << "' (" << n2 << " bytes)\n" << std::flush;

    std::filesystem::path dir_path(relative_start_directory);
    dir_path.append(dirname);
    FileLockGuard guard(dir_path.lexically_normal().string()); // Lock normalized directory path

    // update out_event
    if (out_event) {
        out_event->path = dirname;
    }

    // read and check directory modification time
    uint64_t mod_time_net = 0;
    if (safe_SSL_read(conn, &mod_time_net, sizeof(mod_time_net)) < 0) {
        std::cerr << "[send_recive.cpp:receive_directory_tls] Failed to read directory modification time\n" << std::flush;
        return -1;
    }
    uint64_t mod_time = ntohll(mod_time_net);

    // update out_event
    if (out_event) {
        out_event->timestamp = mod_time;
    }

    auto current_mod_time = get_file_modification_time(dir_path.string());
    if (mod_time <= current_mod_time) {
        std::cerr << "[send_recive.cpp:receive_directory_tls] Received directory is not newer than existing directory. Skipping update for '" << dirname << "'\n" << std::flush;
        return 1; // Not an error, just no update needed
    }


    if (!std::filesystem::create_directories(dir_path)
        && !std::filesystem::is_directory(dir_path)) {
        std::cerr << "[send_recive.cpp:receive_directory_tls] Failed to create directory: " << dirname << "\n" << std::flush;
        return -1;
	}

    if (set_file_modification_time(dir_path.string(), mod_time) < 0) {
        std::cerr << "[send_recive.cpp:receive_directory_tls] Failed to set directory modification time for '" << dirname << "'\n" << std::flush;
        return -1;
    }

    return 0;
}

int receive_delete_directory_tls(std::string relative_start_directory, Connection* conn, Event* out_event) {
    uint64_t name_len = 0;
    int n1 = safe_SSL_read(conn, &name_len, sizeof(name_len));
    if (n1 <= 0) {
        std::cerr << "[send_recive.cpp:receive_delete_directory_tls] Failed to read directory name length\n" << std::flush;
        return -1;
    }
    std::cout << "[send_recive.cpp:receive_delete_directory_tls] Read directory name length: " << n1 << " bytes\n" << std::flush;
    name_len = ntohl(name_len);
    if (name_len == 0 || name_len > 256) {
        std::cerr << "[send_recive.cpp:receive_delete_directory_tls] Invalid directory name length: " << name_len << "\n" << std::flush;
        return -1;
    }

    std::string dirname(name_len, '\0');
    int n2 = safe_SSL_read(conn, dirname.data(), name_len);
    if (n2 <= 0) {
        std::cerr << "[send_recive.cpp:receive_delete_directory_tls] Failed to read directory name\n" << std::flush;
        return -1;
    }
    std::cout << "[send_recive.cpp:receive_delete_directory_tls] Read directory name: '" << dirname << "' (" << n2 << " bytes)\n" << std::flush;

    // construct full directory path
    // Lock the directory path to prevent concurrent modifications
    std::filesystem::path dir_path(relative_start_directory);
    dir_path.append(dirname);
    FileLockGuard guard(dir_path.lexically_normal().string()); // Lock normalized directory path


    // update out_event
    if (out_event) {
        out_event->path = dirname;
    }

    // read and check directory modification time
    uint64_t mod_time_net = 0;
    if (safe_SSL_read(conn, &mod_time_net, sizeof(mod_time_net)) < 0) {
        std::cerr << "[send_recive.cpp:receive_delete_directory_tls] Failed to read directory modification time\n" << std::flush;
        return -1;
    }
    uint64_t mod_time = ntohll(mod_time_net);

    // update out_event
    if (out_event) {
        out_event->timestamp = mod_time;
    }


    auto current_mod_time = get_file_modification_time(dir_path);
    if (mod_time <= current_mod_time) {
        std::cerr << "[send_recive.cpp:receive_delete_directory_tls] Received delete command is not newer than existing directory. Skipping deletion for '" << dirname << "'\n" << std::flush;
        return 1; // Not an error, just no deletion needed
    }

    std::error_code ec;
    auto removed_count = std::filesystem::remove_all(dir_path, ec);
    if (ec) {
        std::cerr << "[send_recive.cpp:receive_delete_directory_tls] Failed to delete directory: " << ec.message() << "\n" << std::flush;
        return -1;
    }

    if (removed_count > 0) {
        std::cout << "[send_recive.cpp:receive_delete_directory_tls] Deleted directory: '" << dir_path.string() << "'\n" << std::flush;
    } else {
        std::cout << "[send_recive.cpp:receive_delete_directory_tls] Delete target already absent, recording delete time: '" << dir_path.string() << "'\n" << std::flush;
    }

    // Record deletion time so stale updates can be rejected later.
    set_delete_mtime(dir_path.string(), mod_time);
    
    return 0;
}

int send_request_handle_pending_event_tls(Connection* conn, std::string relative_start_directory) {
    if (!conn) return -1;
    CommandType command = CommandType::REQUEST_NEXT_PENDING_EVENT;
    if (safe_SSL_write(conn, &command, sizeof(command)) < 0) {
        std::cerr << "[send_recive.cpp:send_request_handle_pending_event_tls] Failed to send request pending events command\n";
        return -1;
    }
    return handle_incoming_event(conn, relative_start_directory);
}

int handle_incoming_event(Connection* conn, std::string relative_start_directory, Event* out_event) {
    // Read command from client
    CommandType command; // Dynamically allocate command to avoid stack issues with large structs and to ensure it remains valid after function returns if needed
    int bytes_read = safe_SSL_read(conn, &command, sizeof(command)); // Read command type (e.g., "UP" for upload)
    if (bytes_read <= 0) {
        std::cerr << "[send_recive.cpp:handle_incoming_event] Failed to read command from client\n";
        return -1;
    }
    if (out_event) {
        out_event->type = command;
    }


    // Handle command, its the programmers responsibility to ensure that commands are all distinct and of a fixed length
    // each in a thread, allows infinite clients to connect and send commands without blocking each other
    // means can be DOSed by opening many connections and sending commands without closing them but clients are certified and assumed to be non-malicious
    
    switch (command) {
        case CommandType::UPLOAD_FILE: {
            std::cout << "[send_recive.cpp:handle_incoming_event] Received upload command from client\n";
            int result = receive_file_tls(relative_start_directory, conn, out_event);
            if (result < 0) {
                std::cerr << "[send_recive.cpp:handle_incoming_event] Failed to receive file\n";
            }
            return result;
        } case CommandType::DELETE_FILE: {
            std::cout << "[send_recive.cpp:handle_incoming_event] Received delete command from client\n";
            int result = receive_delete_file_tls(relative_start_directory, conn, out_event);
            if (result < 0) {
                std::cerr << "[send_recive.cpp:handle_incoming_event] Failed to delete file\n";
            }
            return result;
        } case CommandType::UPLOAD_DIRECTORY: {
            std::cout << "[send_recive.cpp:handle_incoming_event] Received upload directory command from client\n";
            int result = receive_directory_tls(relative_start_directory, conn, out_event);
            if (result < 0) {
                std::cerr << "[send_recive.cpp:handle_incoming_event] Failed to receive directory\n";
            }
            return result;
        } case CommandType::DELETE_DIRECTORY: {
            std::cout << "[send_recive.cpp:handle_incoming_event] Received delete directory command from client\n";
            int result = receive_delete_directory_tls(relative_start_directory, conn, out_event);
            if (result < 0) {
                std::cerr << "[send_recive.cpp:handle_incoming_event] Failed to delete directory\n";
            }
            return result;
        } case CommandType::REQUEST_NEXT_PENDING_EVENT: {
            std::cout << "[send_recive.cpp:handle_incoming_event] Received request pending events command from client\n";
            return 0; // No error, but no event data to return for this command
        } case CommandType::REQUEST_NUMBER_PENDING_EVENTS: {
            std::cout << "[send_recive.cpp:handle_incoming_event] Received request number of pending events command from client\n";
            return 0; // Nothing to be done just close the connetion
        } default: {
            std::cerr << "[send_recive.cpp:handle_incoming_event] Unknown command received from client: " << command << "\n";
            return -1;
        }
    }
}

int handle_send_event(Connection* conn, std::string relative_start_directory, Event* event) {
    // process the event
    if (event->path != "." && event->path != "..") {
        switch (event->type) {
            case CommandType::UPLOAD_FILE: {
                printf("[send_recive.cpp:handle_send_event] Processing upload file event for path: %s\n", event->path.c_str());
                if (send_file_tls(relative_start_directory, event->path, conn) < 0) {
                    std::cerr << "[send_recive.cpp:handle_send_event] Failed to send file: " << event->path << "\n";
                    return -1;
                }
                break;
            } case CommandType::DELETE_FILE: {
                printf("[send_recive.cpp:handle_send_event] Processing delete file event for path: %s\n", event->path.c_str());
                if (send_delete_file_tls(relative_start_directory, event->path, event->timestamp, conn) < 0) {
                    std::cerr << "[send_recive.cpp:handle_send_event] Failed to send delete file request: " << event->path << "\n";
                    return -1;
                }
                break;
            } case CommandType::UPLOAD_DIRECTORY: {
                printf("[send_recive.cpp:handle_send_event] Processing upload directory event for path: %s\n", event->path.c_str());
                if (send_directory_tls(relative_start_directory, event->path, conn) < 0) {
                    std::cerr << "[send_recive.cpp:handle_send_event] Failed to send directory: " << event->path << "\n";
                    return -1;
                }
                break;
            } case CommandType::DELETE_DIRECTORY: {
                printf("[send_recive.cpp:handle_send_event] Processing delete directory event for path: %s\n", event->path.c_str());
                if (send_delete_directory_tls(relative_start_directory, event->path, event->timestamp, conn) < 0) {
                    std::cerr << "[send_recive.cpp:handle_send_event] Failed to send delete directory request: " << event->path << "\n";
                    return -1;
                }
                break;
            } default: {
                std::cerr << "[send_recive.cpp:handle_send_event] Unknown event type: " << event->type << " for path: " << event->path << "\n";
                return -2; // return -2 to indicate unknown event type
            }
        }
        std::cout << "[send_recive.cpp:handle_send_event] Processed event: " << event->type << " for path: " << event->path << "\n";
        return 0;
    } else {
        std::cerr << "[send_recive.cpp:handle_send_event] Skipping event with invalid path: " << event->path << "\n";
        return -3; // return -3 to indicate invalid path
    }
}