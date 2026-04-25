
#ifndef CONNECTION_H
#define CONNECTION_H

#include "send_recive.h"


// Note: try_establish_connection is preferred over establish_connection for general use. Returns pointer to Connection or nullptr on failure.
Connection* establish_connection(const std::string& server_ip, int port);

// Tries to establish a connection, retrying as needed. Returns pointer to Connection or nullptr on failure.
Connection* try_establish_connection(const std::string& server_ip, int port);

// Starts the connection handshake. Returns 0 on success, -1 on failure.
int start_of_connection(Connection* conn);

// Ends the connection and processes any final events. Returns 0 on success, -1 on failure. Internally calls close_connection.
int end_of_connection(Connection* conn);

// Closes the connection and releases resources. Returns 0 on success, -1 on failure.
int close_connection(Connection* conn);

// Shuts down the SSL connection for the given SSL pointer. Returns 0 on success, -1 on failure.
int shutdown_ssl(SSL* ssl);

#endif