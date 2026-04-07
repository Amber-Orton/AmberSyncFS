#include <openssl/types.h>
#include <string>



struct connection{
    SSL* ssl;
    int sock;
    SSL_CTX* ctx;
};
using Connection = struct connection;

int send_file_tls(std::string relative_start_directory, std::string relative_file_path, Connection* conn);
int send_delete_file_tls(std::string relative_file_path, Connection* conn);
int send_directory_tls(std::string relative_directory_path, Connection* conn);
int send_delete_directory_tls(std::string relative_directory_path, Connection* conn);
int receive_file_tls(std::string relative_start_directory, Connection* conn);
int receive_delete_file_tls(std::string relative_start_directory, Connection* conn);
int receive_directory_tls(std::string relative_start_directory, Connection* conn);
int receive_delete_directory_tls(std::string relative_start_directory, Connection* conn);
int safe_SSL_write(Connection* conn, const void* buf, int num);
int safe_SSL_read(Connection* conn, void* buf, int num);