
#ifndef SERVER_H
#define SERVER_H

#include <openssl/ssl.h>
#include <string>

extern int port;

void shutdown_ssl(SSL* ssl);
void close_connection(SSL* ssl, int client_fd);
void end_of_connection(SSL* ssl, int client_fd);

#endif // SERVER_H
