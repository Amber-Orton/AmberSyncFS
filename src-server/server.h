
#ifndef SERVER_H
#define SERVER_H

#include <openssl/ssl.h>

void shutdown_ssl(SSL* ssl);
int receive_file(SSL* ssl, const char* directory);

#endif // SERVER_H
