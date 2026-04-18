
#ifndef CONNECTION_H
#define CONNECTION_H

#include "../src-common/send_recive.h"

int send_file(const std::string& relative_file_path);
int send_delete_file(const std::string& relative_file_path, uint64_t mod_time);
int send_directory(const std::string& relative_directory_path);
int send_delete_directory(const std::string& relative_directory_path, uint64_t mod_time);

Connection* establish_connection(const std::string& server_ip, int port);
Connection* try_establish_connection(const std::string& server_ip, int port);
int start_of_connection(Connection* conn);
int end_of_connection(Connection* conn);
int close_connection(Connection* conn);
int shutdown_ssl(SSL* ssl);

#endif