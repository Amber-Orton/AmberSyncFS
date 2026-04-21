#ifndef SEND_RECIVE_H
#define SEND_RECIVE_H

#include <openssl/types.h>
#include <cstdint>
#include <string>


struct event {
    int id; // used for database events table (will not set id in events table), not needed for file transfer events but can be used when wanted
    std::string client_id;
    std::string type;
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

int send_file_tls(std::string relative_start_directory, std::string relative_file_path, Connection* conn);
int send_delete_file_tls(std::string relative_file_path, uint64_t mod_time, Connection* conn);
int send_directory_tls(std::string relative_start_directory, std::string relative_directory_path, Connection* conn);
int send_delete_directory_tls(std::string relative_directory_path, uint64_t mod_time, Connection* conn);
int receive_file_tls(std::string relative_start_directory, Connection* conn, Event* out_event = nullptr);
int receive_delete_file_tls(std::string relative_start_directory, Connection* conn, Event* out_event = nullptr);
int receive_directory_tls(std::string relative_start_directory, Connection* conn, Event* out_event = nullptr);
int receive_delete_directory_tls(std::string relative_start_directory, Connection* conn, Event* out_event = nullptr);
int handle_incoming_event(Connection* conn, std::string relative_start_directory, Event* out_event = nullptr);

#endif
