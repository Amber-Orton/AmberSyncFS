#include <openssl/types.h>
#include <string>



struct connection{
    SSL* ssl;
    int sock;
    SSL_CTX* ctx;
};
using Connection = struct connection;

int send_file_tls(std::string relative_start_directory, std::string relative_file_path, Connection* conn);
int send_delete_file_tls(std::string relative_file_path, uint64_t mod_time, Connection* conn);
int send_directory_tls(std::string relative_start_directory, std::string relative_directory_path, Connection* conn);
int send_delete_directory_tls(std::string relative_directory_path, uint64_t mod_time, Connection* conn);
int receive_file_tls(std::string relative_start_directory, Connection* conn);
int receive_delete_file_tls(std::string relative_start_directory, Connection* conn);
int receive_directory_tls(std::string relative_start_directory, Connection* conn);
int receive_delete_directory_tls(std::string relative_start_directory, Connection* conn);
int handle_incoming_command(Connection* conn, std::string relative_start_directory);
