
#ifndef CONNECTION_H
#define CONNECTION_H

#include "../src-common/send_recive.h"

Connection* establish_connection(const std::string& server_ip, int port);
Connection* try_establish_connection(const std::string& server_ip, int port);
int start_of_connection(Connection* conn);
int end_of_connection(Connection* conn);
int close_connection(Connection* conn);
int shutdown_ssl(SSL* ssl);

#endif