#include <openssl/types.h>



struct connection{
	SSL* ssl;
	int sock;
	SSL_CTX* ctx;
};
using Connection = struct connection;


Connection* establish_connection(const char* server_ip, int port);
void end_of_connection(Connection* conn);
void close_connection(Connection* conn);
bool send_file_tls(const char* relative_file_path);
void shutdown_ssl(SSL* ssl);