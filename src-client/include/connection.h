#include <openssl/types.h>
#include <string>



struct connection{
	SSL* ssl;
	int sock;
	SSL_CTX* ctx;
};
using Connection = struct connection;

bool send_file_tls(const std::string& relative_file_path);
bool delete_file_tls(const std::string& relative_file_path);

Connection* establish_connection(const std::string& server_ip, int port);
void end_of_connection(Connection* conn);
void close_connection(Connection* conn);
void shutdown_ssl(SSL* ssl);