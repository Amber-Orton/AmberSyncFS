
#ifndef SERVER_H
#define SERVER_H

#include <openssl/ssl.h>
#include <string>
#include "../src-common/send_recive.h"

extern int port;

void shutdown_ssl(SSL* ssl);
void close_connection(Connection* conn);
void end_of_connection(Connection* conn);

#endif // SERVER_H
