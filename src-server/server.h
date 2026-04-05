
#ifndef SERVER_H
#define SERVER_H

#include <openssl/ssl.h>
#include <string>

extern int port;

void shutdown_ssl(SSL* ssl);
void end_of_connection(SSL* ssl, int client_fd);
int receive_file(SSL* ssl, const std::string& directory);
int delete_file(SSL* ssl, const std::string& directory);
int receive_directory(SSL* ssl, const std::string& directory);
int delete_directory(SSL* ssl, const std::string& directory);

#endif // SERVER_H
