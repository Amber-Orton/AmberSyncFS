
#ifndef SERVER_H
#define SERVER_H

#include <openssl/ssl.h>
#include <string>

extern int port;

void shutdown_ssl(SSL* ssl);
int receive_file(SSL* ssl, const std::string& directory);

#endif // SERVER_H
