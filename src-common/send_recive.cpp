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
#include "file_system_evaluator.h"
#define FILE_COLOR "\033[1;36m" // Cyan for send_recive.cpp
#define COLOR_RESET "\033[0m"
#include "../src-client/include/main.h"


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
        CommandType command = CommandType::NOTHING; // Send a command to indicate failure
        if (safe_SSL_write(conn, &command, sizeof(command)) < 0) {
            std::cerr << "[send_recive.cpp:send_file_tls] Failed to send unknown command\n";
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

int send_delete_path_tls(std::string relative_start_directory, std::string relative_directory_path, uint64_t mod_time, Connection* conn) {
    std::filesystem::path full_path(relative_start_directory);
    full_path.append(relative_directory_path);
    FileLockGuard guard(full_path.lexically_normal().string()); // Lock normalized directory path

    CommandType command = CommandType::DELETE_PATH;
    if (safe_SSL_write(conn, &command, sizeof(command)) < 0) { // Simple command to indicate delete path
        std::cerr << "[send_recive.cpp:send_delete_path_tls] Failed to send delete path command\n";
        return -1;
    }

    // Send directory name length and name
    std::string dirname = relative_directory_path; // Send relative path for server dir structure
    uint64_t name_len = htonl(dirname.size());
    if (safe_SSL_write(conn, &name_len, sizeof(name_len)) < 0) {
        std::cerr << "[send_recive.cpp:send_delete_path_tls] Failed to send delete path name length\n";
        return -1;
    }
    if (safe_SSL_write(conn, dirname.c_str(), dirname.size()) < 0) {
        std::cerr << "[send_recive.cpp:send_delete_path_tls] Failed to send delete path name\n";
        return -1;
    }

    uint64_t mod_time_net = htonll(mod_time);
    if (safe_SSL_write(conn, &mod_time_net, sizeof(mod_time_net)) < 0) {
        std::cerr << "[send_recive.cpp:send_delete_path_tls] Failed to send delete path modification time\n";
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
    std::cout << FILE_COLOR << "[send_recive.cpp:receive_file_tls] Read file name length: " << n1 << " bytes" << COLOR_RESET << "\n" << std::flush;
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
    std::cout << FILE_COLOR << "[send_recive.cpp:receive_file_tls] Read file name: '" << filename << "' (" << n2 << " bytes)" << COLOR_RESET << "\n" << std::flush;

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
    std::cout << FILE_COLOR << "[send_recive.cpp:receive_file_tls] Receiving file: '" << filename << "'..." << COLOR_RESET << std::endl << std::flush;
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
    std::cout << FILE_COLOR << "[send_recive.cpp:receive_file_tls] Received file: '" << filename << "' (" << total_bytes << " bytes)" << COLOR_RESET << std::endl << std::flush;
    
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
        std::cout << FILE_COLOR << "[send_recive.cpp:receive_file_tls] Removing temporary file: " << (output_path.string() + ".tmp") << COLOR_RESET << "\n";
        std::filesystem::remove(output_path.string() + ".tmp");
    }
    return 0;
}

int receive_directory_tls(std::string relative_start_directory, Connection* conn, Event* out_event) {
    uint64_t name_len = 0;
    int n1 = safe_SSL_read(conn, &name_len, sizeof(name_len));
    if (n1 <= 0) {
        std::cerr << "[send_recive.cpp:receive_directory_tls] Failed to read directory name length\n" << std::flush;
        return -1;
    }
    std::cout << FILE_COLOR << "[send_recive.cpp:receive_directory_tls] Read directory name length: " << n1 << " bytes" << COLOR_RESET << "\n" << std::flush;
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
    std::cout << FILE_COLOR << "[send_recive.cpp:receive_directory_tls] Read directory name: '" << dirname << "' (" << n2 << " bytes)" << COLOR_RESET << "\n" << std::flush;

    std::filesystem::path dir_path(relative_start_directory);
    dir_path.append(dirname);
    FileLockGuard guard(dir_path);

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
    if (mod_time < current_mod_time) {
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

int receive_delete_path_tls(std::string relative_start_directory, Connection* conn, Event* out_event) {
    uint64_t name_len = 0;
    int n1 = safe_SSL_read(conn, &name_len, sizeof(name_len));
    if (n1 <= 0) {
        std::cerr << "[send_recive.cpp:receive_delete_path_tls] Failed to read path name length\n" << std::flush;
        return -1;
    }
    std::cout << FILE_COLOR << "[send_recive.cpp:receive_delete_path_tls] Read path name length: " << n1 << " bytes" << COLOR_RESET << "\n" << std::flush;
    name_len = ntohl(name_len);
    if (name_len == 0 || name_len > 256) {
        std::cerr << "[send_recive.cpp:receive_delete_path_tls] Invalid path name length: " << name_len << "\n" << std::flush;
        return -1;
    }

    std::string dirname(name_len, '\0');
    int n2 = safe_SSL_read(conn, dirname.data(), name_len);
    if (n2 <= 0) {
        std::cerr << "[send_recive.cpp:receive_delete_path_tls] Failed to read path name\n" << std::flush;
        return -1;
    }
    std::cout << FILE_COLOR << "[send_recive.cpp:receive_delete_path_tls] Read path name: '" << dirname << "' (" << n2 << " bytes)" << COLOR_RESET << "\n" << std::flush;

    // construct full path
    // Lock the path to prevent concurrent modifications
    std::filesystem::path dir_path(relative_start_directory);
    dir_path.append(dirname);
    FileLockGuard guard(dir_path.lexically_normal().string()); // Lock normalized directory path


    // update out_event
    if (out_event) {
        out_event->path = dirname;
    }

    // read and check path modification time
    uint64_t mod_time_net = 0;
    if (safe_SSL_read(conn, &mod_time_net, sizeof(mod_time_net)) < 0) {
        std::cerr << "[send_recive.cpp:receive_delete_path_tls] Failed to read path modification time\n" << std::flush;
        return -1;
    }
    uint64_t mod_time = ntohll(mod_time_net);

    // update out_event
    if (out_event) {
        out_event->timestamp = mod_time;
    }


    auto current_mod_time = get_file_modification_time(dir_path);
    if (mod_time <= current_mod_time) {
        std::cerr << "[send_recive.cpp:receive_delete_path_tls] Received delete command is not newer than existing path. Skipping deletion for '" << dirname << "'\n" << std::flush;
        return 1; // Not an error, just no deletion needed
    }

    std::error_code ec;
    auto removed_count = std::filesystem::remove_all(dir_path, ec);
    if (ec) {
        std::cerr << "[send_recive.cpp:receive_delete_path_tls] Failed to delete path: " << ec.message() << "\n" << std::flush;
        return -1;
    }

    if (removed_count > 0) {
        std::cout << FILE_COLOR << "[send_recive.cpp:receive_delete_path_tls] Deleted path: '" << dir_path.string() << "'" << COLOR_RESET << "\n" << std::flush;
    } else {
        std::cout << FILE_COLOR << "[send_recive.cpp:receive_delete_path_tls] Delete target already absent, recording delete time: '" << dir_path.string() << "'" << COLOR_RESET << "\n" << std::flush;
    }

    // Record deletion time so stale updates can be rejected later.
    set_delete_mtime(dir_path.string(), mod_time);
    
    return 0;
}

int send_request_number_pending_events_tls(Connection* conn) {
    if (!conn) return -1;
    CommandType command = CommandType::REQUEST_NUMBER_PENDING_EVENTS;
    if (safe_SSL_write(conn, &command, sizeof(command)) < 0) {
        std::cerr << "[send_recive.cpp:send_request_number_pending_events_tls] Failed to send request number of pending events command\n";
        return -1;
    }
    return 0;
}

int send_handle_request_update_for_path(Connection* conn, std::string relative_start_directory, std::string relative_path) {
    if (!conn) return -1;
    CommandType command = CommandType::REQUEST_UPDATE_FOR_PATH;
    if (safe_SSL_write(conn, &command, sizeof(command)) < 0) {
        std::cerr << "[send_recive.cpp:send_handle_request_update_for_path] Failed to send request handle update for path command\n";
        return -1;
    }
    uint64_t name_len = relative_path.length();
    if (safe_SSL_write(conn, &name_len, sizeof(name_len)) < 0) {
        std::cerr << "[send_recive.cpp:send_handle_request_update_for_path] Failed to send path name length\n" << std::flush;
        return -1;
    }
    if (safe_SSL_write(conn, relative_path.c_str(), name_len) < 0) {
        std::cerr << "[send_recive.cpp:send_handle_request_update_for_path] Failed to send path name\n" << std::flush;
        return -1;
    }
    return handle_incoming_event(conn, relative_start_directory);
}

int send_handle_request_pending_event_tls(Connection* conn, std::string relative_start_directory) {
    if (!conn) return -1;
    CommandType command = CommandType::REQUEST_NEXT_PENDING_EVENT;
    if (safe_SSL_write(conn, &command, sizeof(command)) < 0) {
        std::cerr << "[send_recive.cpp:send_handle_request_pending_event_tls] Failed to send request pending events command\n";
        return -1;
    }
    return handle_incoming_event(conn, relative_start_directory);
}

int receive_handle_request_pending_event_tls(std::string relative_start_directory, Connection* conn, Event* out_event) {
    // Client requested a pending event
    if (auto event = get_and_set_in_progress_next_event(out_event ? out_event->client_id : "")) {               
        // sned the event to the client
        if (event.has_value()) {
            if (handle_send_event(conn, relative_start_directory, &event.value()) < 0) {
                std::cerr << "[server.cpp:main] Failed to send pending event to client: " << out_event->client_id << "\n";
                reset_in_progress_event(event->id); // re-add the event to the database to retry later
                return -1;
            }
            return 0;
        } else {
            std::cerr << "[server.cpp:main] No pending event found for client: " << out_event->client_id << "\n";
            event->type = CommandType::NOTHING; // set to nothing command to indicate no event found, client will handle this by just closing the connection
            handle_send_event(conn, relative_start_directory, &event.value()); // send the nothing command response to the client
            return 1;
        }
    }
    return -1; // return -1 if there was an error or no event was found
}

int receive_handle_request_update_for_path(std::string relative_start_directory, Connection* conn) {
    // Client requested an update for a specific path, read the path from the client and send an update for that path
    uint64_t name_len = 0;
    if (safe_SSL_read(conn, &name_len, sizeof(name_len)) < 0) {
        std::cerr << "[send_recive.cpp:receive_handle_request_update_for_path] Failed to read path name length\n" << std::flush;
        return -1;
    }
    std::string relative_path(name_len, '\0');
    if (safe_SSL_read(conn, &relative_path[0], name_len) < 0) {
        std::cerr << "[send_recive.cpp:receive_handle_request_update_for_path] Failed to read path name\n" << std::flush;
        return -1;
    }
    
    // check if its a file or directory or doesnt exist and send the appropriate response
    std::filesystem::path full_path(relative_start_directory);
    full_path.append(relative_path);
    if (std::filesystem::exists(full_path)) {
        if (std::filesystem::is_directory(full_path)) {
            return send_directory_tls(relative_start_directory, relative_path, conn);
        } else if (std::filesystem::is_regular_file(full_path)) {
            return send_file_tls(relative_start_directory, relative_path, conn);
        } else {
            std::cerr << "[send_recive.cpp:receive_handle_request_update_for_path] Path exists but is not a regular file or directory: " << full_path.string() << "\n";
            return -1;
        }
    } else {
        // Path doesn't exist, send a delete path command to ensure client deletes it if it exists on their end
        return send_delete_path_tls(relative_start_directory, relative_path, get_file_modification_time(full_path), conn);
    }
}

int receive_handle_request_directory_structure(std::string relative_start_directory, Connection* conn) {
    std::string dir_snapshot = generate_snapshot(relative_start_directory);
    uint64_t dir_snapshot_size = dir_snapshot.size();
    uint64_t dir_snapshot_size_net = htonll(dir_snapshot_size);
    if (safe_SSL_write(conn, &dir_snapshot_size_net, sizeof(dir_snapshot_size_net)) < 0) {
        std::cerr << "[send_recive.cpp:receive_handle_request_directory_structure] Failed to send directory structure size\n";
        return -1;
    }
    if (safe_SSL_write(conn, dir_snapshot.c_str(), dir_snapshot_size) < 0) {
        std::cerr << "[send_recive.cpp:receive_handle_request_directory_structure] Failed to send directory structure data\n";
        return -1;
    }
    return 0;
}

int send_request_directory_structure(Connection* conn, void* out_response) {
    if (!out_response) {
        return -1;
    }
    if (!conn) return -1;
    CommandType command = CommandType::REQUEST_DIRECTORY_STRUCTURE;
    if (safe_SSL_write(conn, &command, sizeof(command)) < 0) {
        std::cerr << "[send_recive.cpp:send_request_directory_structure] Failed to send request directory structure command\n";
        return -1;
    }
    uint64_t dir_struct_size;
    if (safe_SSL_read(conn, &dir_struct_size, sizeof(dir_struct_size)) < 0) {
        std::cerr << "[send_recive.cpp:send_request_directory_structure] Failed to read directory structure size\n";
        return -1;
    }
    dir_struct_size = ntohll(dir_struct_size);

    *static_cast<std::string*>(out_response) = std::string(dir_struct_size, '\0');
    if (safe_SSL_read(conn, (*static_cast<std::string*>(out_response)).data(), dir_struct_size) < 0) {
        std::cerr << "[send_recive.cpp:send_request_directory_structure] Failed to read directory structure data\n";
        return -1;
    }
    return 0;
}

int handle_incoming_event(Connection* conn, std::string relative_start_directory, Event* out_event) {
    // Read command from client
    CommandType command; // Dynamically allocate command to avoid stack issues with large structs and to ensure it remains valid after function returns if needed
    int bytes_read = safe_SSL_read(conn, &command, sizeof(command)); // Read command type (e.g., "UP" for upload)
    if (bytes_read <= 0) {
        std::cerr << "[send_recive.cpp:handle_incoming_event] Failed to read command\n";
        return -1;
    }
    Event event;
    if (!out_event) {
        out_event = &event; // Use local event if out_event is null
    }
    out_event->type = command;


    // Handle command, its the programmers responsibility to ensure that commands are all distinct and of a fixed length
    // each in a thread, allows infinite clients to connect and send commands without blocking each other
    // means can be DOSed by opening many connections and sending commands without closing them but clients are certified and assumed to be non-malicious
    
    int result = -1;

    switch (command) {
        case CommandType::UPLOAD_FILE: {
            std::cout << FILE_COLOR << "[send_recive.cpp:handle_incoming_event] Received upload command" << COLOR_RESET << "\n";
            result = receive_file_tls(relative_start_directory, conn, out_event);
            if (result < 0) {
                std::cerr << "[send_recive.cpp:handle_incoming_event] Failed to receive file\n";
            }
            break;
        } case CommandType::UPLOAD_DIRECTORY: {
            std::cout << FILE_COLOR << "[send_recive.cpp:handle_incoming_event] Received upload directory command" << COLOR_RESET << "\n";
            result = receive_directory_tls(relative_start_directory, conn, out_event);
            if (result < 0) {
                std::cerr << "[send_recive.cpp:handle_incoming_event] Failed to receive directory\n";
            }
            break;
        } case CommandType::DELETE_PATH: {
            std::cout << FILE_COLOR << "[send_recive.cpp:handle_incoming_event] Received delete path command" << COLOR_RESET << "\n";
            result = receive_delete_path_tls(relative_start_directory, conn, out_event);
            if (result < 0) {
                std::cerr << "[send_recive.cpp:handle_incoming_event] Failed to delete path\n";
            }
            break;
        } case CommandType::REQUEST_NEXT_PENDING_EVENT: {
            std::cout << FILE_COLOR << "[send_recive.cpp:handle_incoming_event] Received request pending events command" << COLOR_RESET << "\n";
            result = receive_handle_request_pending_event_tls(relative_start_directory, conn, out_event);
            if (result < 0) {
                std::cerr << "[send_recive.cpp:handle_incoming_event] Failed to process request pending events command\n";
            }
            break;
        } case CommandType::REQUEST_NUMBER_PENDING_EVENTS: {
            std::cout << FILE_COLOR << "[send_recive.cpp:handle_incoming_event] Received request number of pending events command from client" << COLOR_RESET << "\n";
            result = 0; // No error, but no event data to return for this command, the server will respond to this command separately with the number of pending events
            break;
        } case CommandType::REQUEST_UPDATE_FOR_PATH: {
            std::cout << FILE_COLOR << "[send_recive.cpp:handle_incoming_event] Received request update for path command from client" << COLOR_RESET << "\n";
            result = receive_handle_request_update_for_path(relative_start_directory, conn);
            if (result < 0) {
                std::cerr << "[send_recive.cpp:handle_incoming_event] Failed to process request update for path command\n";
            }
            break;
        } case CommandType::REQUEST_DIRECTORY_STRUCTURE: {
            std::cout << FILE_COLOR << "[send_recive.cpp:handle_incoming_event] Received request directory structure command from client" << COLOR_RESET << "\n";
            result = receive_handle_request_directory_structure(relative_start_directory, conn);
            if (result < 0) {
                std::cerr << "[send_recive.cpp:handle_incoming_event] Failed to process request directory structure command\n";
            }
            break;
        } case CommandType::NOTHING: {
            std::cout << FILE_COLOR << "[send_recive.cpp:handle_incoming_event] Received nothing command from client, no event to process" << COLOR_RESET << "\n";
            result = 2; // No error, but no event data to return for this command
            break;
        } default: {
            std::cerr << "[send_recive.cpp:handle_incoming_event] Unknown command received from client: " << command << "\n";
            result = -1;
            break;
        }
    }
    // TODO make an event if we have a newer version of a file than the client probs to be done in the server code
    if (result == 1) { // result of 1 indicates update not accepted because incoming is not newer than existing
        std::cout << FILE_COLOR << "[send_recive.cpp:handle_incoming_event] No update needed for event type: " << command << " creating event to send update other way" << COLOR_RESET << "\n";
        auto path = std::filesystem::path(relative_start_directory).append(out_event->path);
        CommandType event_type;
        if (std::filesystem::exists(path)) {
            if (std::filesystem::is_directory(path)) {
                event_type = CommandType::UPLOAD_DIRECTORY;
            } else if (std::filesystem::is_regular_file(path)) {
                event_type = CommandType::UPLOAD_FILE;
            } else {
                std::cerr << "[send_recive.cpp:handle_incoming_event] Path exists but is not a regular file or directory: " << path.string() << "\n";
                return -1;
            }
        } else {
            event_type = CommandType::DELETE_PATH;
        }
        auto mod_time = get_file_modification_time(path);
        create_event(event_type, out_event->path, mod_time, out_event->client_id);
        // if client then wake an event handler
        #ifdef SYNCFS_CLIENT
            events_cv.notify_one();
        #endif
    }
    return result;
}

int handle_send_event(Connection* conn, std::string relative_start_directory, Event* event, void* out_response) {
    // process the event
    if (event->path != "." && event->path != "..") {
        switch (event->type) {
            case CommandType::UPLOAD_FILE: {
                std::cout << FILE_COLOR << "[send_recive.cpp:handle_send_event] Processing upload file event for path: " << event->path << COLOR_RESET << "\n";
                int result = send_file_tls(relative_start_directory, event->path, conn);
                if (result < 0) {
                    std::cerr << "[send_recive.cpp:handle_send_event] Failed to send file: " << event->path << "\n";
                    return -1;
                } else if (result == 1) {
                    std::cerr << "[send_recive.cpp:handle_send_event] File not sent as it doesn't exist\n";
                    return result;
                }
                break;
            } case CommandType::DELETE_PATH: {
                std::cout << FILE_COLOR << "[send_recive.cpp:handle_send_event] Processing delete path event for path: " << event->path << COLOR_RESET << "\n";
                if (send_delete_path_tls(relative_start_directory, event->path, event->timestamp, conn) < 0) {
                    std::cerr << "[send_recive.cpp:handle_send_event] Failed to send delete path request: " << event->path << "\n";
                    return -1;
                }
                break;
            } case CommandType::UPLOAD_DIRECTORY: {
                std::cout << FILE_COLOR << "[send_recive.cpp:handle_send_event] Processing upload directory event for path: " << event->path << COLOR_RESET << "\n";
                if (send_directory_tls(relative_start_directory, event->path, conn) < 0) {
                    std::cerr << "[send_recive.cpp:handle_send_event] Failed to send directory: " << event->path << "\n";
                    return -1;
                }
                break;
            } case CommandType::REQUEST_NEXT_PENDING_EVENT: {
                std::cout << FILE_COLOR << "[send_recive.cpp:handle_send_event] Processing request next pending event command" << COLOR_RESET << "\n";
                if (send_handle_request_pending_event_tls(conn, relative_start_directory) < 0) {
                    std::cerr << "[send_recive.cpp:handle_send_event] Failed to send request handle pending event command\n";
                    return -1;
                }
                return 0;
            } case CommandType::REQUEST_NUMBER_PENDING_EVENTS: {
                std::cout << FILE_COLOR << "[send_recive.cpp:handle_send_event] Processing request number of pending events command" << COLOR_RESET << "\n";
                if (send_request_number_pending_events_tls(conn) < 0) {
                    std::cerr << "[send_recive.cpp:handle_send_event] Failed to send request number of pending events command\n";
                    return -1;
                }
                return 0;
            } case CommandType::REQUEST_UPDATE_FOR_PATH: {
                std::cout << FILE_COLOR << "[send_recive.cpp:handle_send_event] Processing request update for path command for path: " << event->path << COLOR_RESET << "\n";
                if (send_handle_request_update_for_path(conn, relative_start_directory, event->path) < 0) {
                    std::cerr << "[send_recive.cpp:handle_send_event] Failed to process request update for path command for path: " << event->path << "\n";
                    return -1;
                }
                return 0;
            } case CommandType::REQUEST_DIRECTORY_STRUCTURE: {
                std::cout << FILE_COLOR << "[send_recive.cpp:handle_send_event] Processing request directory structure command" << COLOR_RESET << "\n";
                if (send_request_directory_structure(conn, out_response) < 0) {
                    std::cerr << "[send_recive.cpp:handle_send_event] Failed to process request directory structure command\n";
                    return -1;
                }
                return 0;
            } default: {
                std::cerr << "[send_recive.cpp:handle_send_event] Unknown event type: " << event->type << " for path: " << event->path << "\n";
                return -2; // return -2 to indicate unknown event type
            }
        }
        std::cout << FILE_COLOR << "[send_recive.cpp:handle_send_event] Processed event: " << event->type << " for path: " << event->path << COLOR_RESET << "\n";
        return 0;
    } else {
        std::cerr << "[send_recive.cpp:handle_send_event] Skipping event with invalid path: " << event->path << "\n";
        return -3; // return -3 to indicate invalid path
    }
}