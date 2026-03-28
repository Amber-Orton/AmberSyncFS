
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <openssl/ssl.h>
#include <openssl/err.h>

int main() {
    int port = 12345;

    // 1. Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        std::cerr << "Failed to create SSL_CTX\n";
        return 1;
    }
    if (SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, "server.key", SSL_FILETYPE_PEM) <= 0) {
        std::cerr << "Failed to load cert/key\n";
        SSL_CTX_free(ctx);
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, 1);
    std::cout << "TLS server listening on port " << port << std::endl;

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) continue;

        SSL* ssl = SSL_new(ctx);
        SSL_set_fd(ssl, client_fd);
        if (SSL_accept(ssl) <= 0) {
            std::cerr << "SSL handshake failed\n";
            SSL_free(ssl);
            close(client_fd);
            continue;
        }

        uint32_t name_len = 0;
        SSL_read(ssl, &name_len, sizeof(name_len));
        name_len = ntohl(name_len);
        if (name_len == 0 || name_len > 256) {
            SSL_free(ssl);
            close(client_fd);
            continue;
        }

        char filename[257] = {0};
        SSL_read(ssl, filename, name_len);
        filename[name_len] = '\0';

        std::ofstream outfile(filename, std::ios::binary);
        if (!outfile) {
            SSL_free(ssl);
            close(client_fd);
            continue;
        }

        char buffer[4096];
        int bytes;
        while ((bytes = SSL_read(ssl, buffer, sizeof(buffer))) > 0) {
            outfile.write(buffer, bytes);
        }
        std::cout << "Received file: " << filename << std::endl;
        outfile.close();

        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_fd);
    }
    SSL_CTX_free(ctx);
    close(server_fd);
    return 0;
}
