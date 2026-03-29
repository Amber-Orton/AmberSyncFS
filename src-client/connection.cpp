#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <ostream>
#include "connection.h"
#include "main.h"


using Connection = struct connection;

bool send_file_tls(const char* relative_file_path) {
    // Open file
    std::string file_path = std::string(track_root) + "/" + relative_file_path;
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file\n";
        return false;
    }

    // Establish TLS connection
    Connection* conn = establish_connection(server_ip, server_port);
    if (!conn) {
        std::cerr << "Failed to open TLS connection\n";
        return false;
    }

    SSL_write(conn->ssl, "UP", 2); // Simple command to indicate upload

    // Send file name length and name
    std::string filename = relative_file_path; // Send relative path for server dir structure
    uint32_t name_len = htonl(filename.size());
    SSL_write(conn->ssl, &name_len, sizeof(name_len));
    SSL_write(conn->ssl, filename.c_str(), filename.size());

    // Send file data
    char buffer[4096];
    while (file) {
        file.read(buffer, sizeof(buffer));
        std::streamsize bytes = file.gcount();
        if (bytes > 0) {
            SSL_write(conn->ssl, buffer, bytes);
        }
    }

    // Ensure file is flushed and closed before shutting down SSL
    file.close();

    end_of_connection(conn);
    return true;
}

Connection* establish_connection(const char* server_ip, int port) {
    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        std::cerr << "Failed to create SSL_CTX\n";
        return nullptr;
    }


    // Load CA or server certificate for validation
    if (SSL_CTX_load_verify_locations(ctx, "certs/ca.crt", nullptr) != 1) {
        std::cerr << "Failed to load CA/server certificate for validation\n";
        SSL_CTX_free(ctx);
        return nullptr;
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);

    // Load client certificate and private key for mutual authentication
    if (SSL_CTX_use_certificate_file(ctx, "certs/client.crt", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, "certs/client.key", SSL_FILETYPE_PEM) <= 0) {
        std::cerr << "Failed to load client cert/key for mutual TLS\n";
        SSL_CTX_free(ctx);
        return nullptr;
    }

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation failed\n";
        SSL_CTX_free(ctx);
        return nullptr;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed\n";
        close(sock);
        SSL_CTX_free(ctx);
        return nullptr;
    }

    // Wrap socket with SSL
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);

    if (SSL_connect(ssl) <= 0) {
        std::cerr << "SSL handshake failed\n";
        SSL_free(ssl);
        close(sock);
        SSL_CTX_free(ctx);
        return nullptr;
    }

    // Verify server certificate
    long verify_result = SSL_get_verify_result(ssl);
    if (verify_result != X509_V_OK) {
        std::cerr << "Server certificate verification failed: " << X509_verify_cert_error_string(verify_result) << "\n";
        shutdown_ssl(ssl);
        SSL_free(ssl);
        close(sock);
        SSL_CTX_free(ctx);
        return nullptr;
    }
    auto conn = new Connection{ssl, sock, ctx};
    return conn;
}

void end_of_connection(Connection* conn) {
    close_connection(conn);
}

void close_connection(Connection* conn) {
    if (!conn) return;

    shutdown_ssl(conn->ssl);

    SSL_free(conn->ssl);
    close(conn->sock);
    SSL_CTX_free(conn->ctx);
    delete conn;
}
    

void shutdown_ssl(SSL* ssl) {
    int shutdown_result;
    do {
        shutdown_result = SSL_shutdown(ssl);
        if (shutdown_result != 1 && shutdown_result != 0) {
            std::cerr << "SSL shutdown failed\n";
        }
    } while (shutdown_result == 0);
}