#ifndef SEND_RECIVE_H
#define SEND_RECIVE_H

#include <openssl/types.h>
#include <cstdint>
#include <string>
#include "command.h"


struct event {
    int id; // used for database events table (will not set id in events table), not needed for file transfer events but can be used when wanted
    std::string client_id;
    CommandType type;
    std::string path; // relative path from the tracked root directory
    uint64_t timestamp;
};
using Event = struct event;


struct connection{
    SSL* ssl;
    int sock;
    SSL_CTX* ctx;
};
using Connection = struct connection;


// Returns 0 on success, -1 on failure, and 1 if the file was not updated because the file did not exist
int send_file_tls(std::string relative_start_directory, std::string relative_file_path, Connection* conn);

// Returns 0 on success, -1 on failure, and 1 if the file was not deleted because the file did not exist
int send_delete_file_tls(std::string relative_start_directory, std::string relative_file_path, uint64_t mod_time, Connection* conn);

// Returns 0 on success, -1 on failure
int send_directory_tls(std::string relative_start_directory, std::string relative_directory_path, Connection* conn);

// Returns 0 on success, -1 on failure
int send_delete_directory_tls(std::string relative_start_directory, std::string relative_directory_path, uint64_t mod_time, Connection* conn);

// Returns 0 on success, -1 on failure, and 1 if the file was not updated because the incoming file was not newer than the existing file (but still read and discarded the incoming data to clear the SSL buffer)
int receive_file_tls(std::string relative_start_directory, Connection* conn, Event* out_event = nullptr);

// Returns 0 on success, -1 on failure, and 1 if the file was not deleted because the incoming delete command was not newer than the existing file (but still read and discarded the incoming data to clear the SSL buffer)
int receive_delete_file_tls(std::string relative_start_directory, Connection* conn, Event* out_event = nullptr);

// Returns 0 on success, -1 on failure, and 1 if the directory was not updated because the incoming directory was not newer than the existing directory (but still read and discarded the incoming data to clear the SSL buffer)
int receive_directory_tls(std::string relative_start_directory, Connection* conn, Event* out_event = nullptr);

// Returns 0 on success, -1 on failure, and 1 if the directory was not deleted because the incoming delete command was not newer than the existing directory (but still read and discarded the incoming data to clear the SSL buffer)
int receive_delete_directory_tls(std::string relative_start_directory, Connection* conn, Event* out_event = nullptr);

// Returns 0 on success, -1 on failure. Used to request handling of pending events from the server.
int send_request_handle_pending_event_tls(Connection* conn, std::string relative_start_directory);

// Returns 0 on success, -1 on failure. Handles an incoming event from the client.
int handle_incoming_event(Connection* conn, std::string relative_start_directory, Event* out_event = nullptr);

// Returns 0 on success, -1 on failure, -2 for unknown event type, -3 for invalid path. Handles sending an event to the server.
int handle_send_event(Connection* conn, std::string relative_start_directory, Event* event);


#endif
