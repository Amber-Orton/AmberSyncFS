#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <openssl/ssl.h>
#include <openssl/err.h>

bool send_file_tls(const char* server_ip, int port, const char* file_path) {
    // 1. Open file
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file\n";
        return false;
    }

    // 2. Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        std::cerr << "Failed to create SSL_CTX\n";
        return false;
    }


    // Load CA or server certificate for validation
    if (SSL_CTX_load_verify_locations(ctx, "server.crt", nullptr) != 1) {
        std::cerr << "Failed to load CA/server certificate for validation\n";
        SSL_CTX_free(ctx);
        return false;
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);

    // Load client certificate and private key for mutual authentication
    if (SSL_CTX_use_certificate_file(ctx, "client.crt", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, "client.key", SSL_FILETYPE_PEM) <= 0) {
        std::cerr << "Failed to load client cert/key for mutual TLS\n";
        SSL_CTX_free(ctx);
        return false;
    }

    // 3. Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation failed\n";
        SSL_CTX_free(ctx);
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed\n";
        close(sock);
        SSL_CTX_free(ctx);
        return false;
    }

    // 4. Wrap socket with SSL
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);

    if (SSL_connect(ssl) <= 0) {
        std::cerr << "SSL handshake failed\n";
        SSL_free(ssl);
        close(sock);
        SSL_CTX_free(ctx);
        return false;
    }

    // Verify server certificate
    long verify_result = SSL_get_verify_result(ssl);
    if (verify_result != X509_V_OK) {
        std::cerr << "Server certificate verification failed: " << X509_verify_cert_error_string(verify_result) << "\n";
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(sock);
        SSL_CTX_free(ctx);
        return false;
    }

    // 5. Send file name length and name
    std::string filename = file_path;
    size_t pos = filename.find_last_of("/\\");
    std::string just_name = (pos == std::string::npos) ? filename : filename.substr(pos + 1);
    uint32_t name_len = htonl(just_name.size());
    SSL_write(ssl, &name_len, sizeof(name_len));
    SSL_write(ssl, just_name.c_str(), just_name.size());

    // 6. Send file data
    char buffer[4096];
    while (file) {
        file.read(buffer, sizeof(buffer));
        std::streamsize bytes = file.gcount();
        if (bytes > 0) {
            SSL_write(ssl, buffer, bytes);
        }
    }

    // 7. Clean up
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);
    file.close();
    std::cout << "File sent!\n";
    return true;
}

int main() {
    send_file_tls("127.0.0.1", 12345, "test.txt");
    return 0;
}
