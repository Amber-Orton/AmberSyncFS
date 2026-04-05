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
#include <chrono>
#include <mutex>
#include <algorithm>
#include <thread>


using Connection = struct connection;

std::atomic<bool> is_connected{false};
std::atomic<bool> connection_try_in_progress{false};
auto connection_retry_timeout = std::chrono::seconds(1);
auto last_connection_attempt_time = std::chrono::steady_clock::now() - std::chrono::seconds(10);
const auto max_retry_timeout = std::chrono::seconds(60);

bool send_file_tls(const std::string& relative_file_path) {
    // Open file
    std::string file_path = track_root + "/" + relative_file_path;
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file\n";
        return false;
    }

    // Establish TLS connection
    Connection* conn = try_establish_connection(server_ip, server_port);
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

bool delete_file_tls(const std::string& relative_file_path) {
    // Establish TLS connection
    Connection* conn = try_establish_connection(server_ip, server_port);
    if (!conn) {
        std::cerr << "Failed to open TLS connection\n";
        return false;
    }

    SSL_write(conn->ssl, "DL", 2); // Simple command to indicate delete
    
    // Send file name length and name
    std::string filename = relative_file_path; // Send relative path for server dir structure
    uint32_t name_len = htonl(filename.size());
    SSL_write(conn->ssl, &name_len, sizeof(name_len));
    SSL_write(conn->ssl, filename.c_str(), filename.size());

    end_of_connection(conn);
    return true;
}


Connection* try_establish_connection(const std::string& server_ip, int port) {
    while (true) {
        if (is_connected.load()) {
            auto conn = establish_connection(server_ip, port);
            if (conn != nullptr) {
                return conn;
            } else {
                is_connected.store(false);
            }
        }
        auto time_since_last_attempt = std::chrono::steady_clock::now() - last_connection_attempt_time;
        if (time_since_last_attempt < connection_retry_timeout) {
            std::cout << "Waiting before next connection attempt for " << (connection_retry_timeout - time_since_last_attempt).count() / 1000000000 << " s\n";
            std::this_thread::sleep_for((connection_retry_timeout - time_since_last_attempt) * (1 + rand() % 5)); // *1.1 so one thread leaves before rest, this does not need to be strict
        }
        if (connection_try_in_progress.exchange(true)) { // other thread is trying to connect
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * (1 + rand() % 5)));
            continue; // check connection status again after waiting, if other thread succeeded we can just return, otherwise we will try to connect ourselves
        }
        if (is_connected.load()) {
            connection_try_in_progress.store(false);
            continue; // another thread established connection while we were waiting or processing.
        }
        auto conn = establish_connection(server_ip, port);
        if (conn != nullptr) {
            is_connected.store(true);
            connection_retry_timeout = std::chrono::seconds(1); // reset retry timeout after successful connection
            last_connection_attempt_time = std::chrono::steady_clock::now();
            connection_try_in_progress.store(false);
            return conn;
        } else {
            connection_retry_timeout = std::min(connection_retry_timeout * 2, max_retry_timeout);
            last_connection_attempt_time = std::chrono::steady_clock::now();
            connection_try_in_progress.store(false);
        }
    }
}


Connection* establish_connection(const std::string& server_ip, int port) {
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
    inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

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