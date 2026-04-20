
#ifndef CONNECTION_H
#define CONNECTION_H

#include "../src-common/send_recive.h"

int send_file(const Event& event);
int send_delete_file(const Event& event);
int send_directory(const Event& event);
int send_delete_directory(const Event& event);

Connection* establish_connection(const std::string& server_ip, int port);
Connection* try_establish_connection(const std::string& server_ip, int port);
int start_of_connection(Connection* conn);
int end_of_connection(Connection* conn);
int close_connection(Connection* conn);
int shutdown_ssl(SSL* ssl);

#endif