#ifndef SERVER_H
#define SERVER_H

#include <openssl/ssl.h>
#include <string>
#include "send_recive.h"

extern int port;

// Shuts down the SSL connection for the given SSL pointer.
void shutdown_ssl(SSL* ssl);

// Returns 0 on success, -1 on failure. Starts a connection handshake.
int start_of_connection(Connection* conn);

// Closes the connection and releases resources.
void close_connection(Connection* conn);

// Ends the connection and processes any final events for the given Event.
void end_of_connection(Connection* conn, Event& event);

#endif // SERVER_H
