#include <openssl/types.h>



struct connection{
	SSL* ssl;
	int sock;
	SSL_CTX* ctx;
};
using Connection = struct connection;


Connection* open_connection(const char* server_ip, int port);
void end_of_connection(Connection* conn);
bool send_file_tls(const char* server_ip, int port, const char* track_root, const char* relative_file_path);
void shutdown_ssl(SSL* ssl);