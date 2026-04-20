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
#include <condition_variable>
#include "../src-common/send_recive_helper.h"



std::atomic<bool> is_connected{false};
auto connection_retry_timeout = std::chrono::seconds(1);
auto last_connection_attempt_time = std::chrono::steady_clock::now() - std::chrono::seconds(10);
const auto max_retry_timeout = std::chrono::seconds(60);

std::mutex connection_try_mutex;




int send_file(const Event& event) {
    // Establish TLS connection
    auto* conn = try_establish_connection(server_ip, server_port);
    if (!conn) {
        std::cerr << "Failed to open TLS connection\n";
        return -1;
    }

    int sucess = start_of_connection(conn);

    if (sucess < 0) {
        std::cerr << "Failed to initialize server connection\n";
        close_connection(conn);
        return sucess;
    }

    sucess = send_file_tls(track_root, event.path, conn);

    if (sucess >= 0) {
        auto eoc_result = end_of_connection(conn);
        return eoc_result != 0 ? eoc_result : sucess;
    } else {
        std::cerr << "Failed to send file\n";
        auto close_result = close_connection(conn);
        return close_result != 0 ? close_result : sucess;
    }
}

int send_delete_file(const Event& event) {
    // Establish TLS connection
    auto* conn = try_establish_connection(server_ip, server_port);
    if (!conn) {
        std::cerr << "Failed to open TLS connection\n";
        return -1;
    }

    int sucess = start_of_connection(conn);

    if (sucess < 0) {
        std::cerr << "Failed to initialize server connection\n";
        close_connection(conn);
        return sucess;
    }

    sucess = send_delete_file_tls(event.path, event.timestamp, conn);

    if (sucess >= 0) {
        auto eoc_result = end_of_connection(conn);
        return eoc_result != 0 ? eoc_result : sucess;
    } else {
        std::cerr << "Failed to send delete file\n";
        auto close_result = close_connection(conn);
        return close_result != 0 ? close_result : sucess;
    }
}

int send_directory(const Event& event) {
    // Establish TLS connection
    auto* conn = try_establish_connection(server_ip, server_port);
    if (!conn) {
        std::cerr << "Failed to open TLS connection\n";
        return -1;
    }

    int sucess = start_of_connection(conn);

    if (sucess < 0) {
        std::cerr << "Failed to initialize server connection\n";
        close_connection(conn);
        return sucess;
    }

    sucess = send_directory_tls(track_root, event.path, conn);

    if (sucess >= 0) {
        auto eoc_result = end_of_connection(conn);
        return eoc_result != 0 ? eoc_result : sucess;
    } else {
        std::cerr << "Failed to send directory\n";
        auto close_result = close_connection(conn);
        return close_result != 0 ? close_result : sucess;
    }
}

int send_delete_directory(const Event& event) {
    // Establish TLS connection
    auto* conn = try_establish_connection(server_ip, server_port);
    if (!conn) {
        std::cerr << "Failed to open TLS connection\n";
        return -1;
    }

    int sucess = start_of_connection(conn);

    if (sucess < 0) {
        std::cerr << "Failed to initialize server connection\n";
        close_connection(conn);
        return sucess;
    }

    sucess = send_delete_directory_tls(event.path, event.timestamp, conn);

    if (sucess >= 0) {
        auto eoc_result = end_of_connection(conn);
        return eoc_result != 0 ? eoc_result : sucess;
    } else {
        std::cerr << "Failed to send delete directory\n";
        auto close_result = close_connection(conn);
        return close_result != 0 ? close_result : sucess;
    }
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

        // lock notification
        std::lock_guard<std::mutex> lock(connection_try_mutex);
        if (is_connected.load()) {
            continue; // another thread connected while we were waiting for the lock, just loop back and check connection status again
        }
        while (true) {
            // sleep if whithin retry time
            auto time_since_last_attempt = std::chrono::steady_clock::now() - last_connection_attempt_time;
            if (time_since_last_attempt < connection_retry_timeout) {
                double jitter = 0.9 + (rand() % 21) / 100.0; // 0.90 to 1.10
                auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>((connection_retry_timeout - time_since_last_attempt) * jitter);
                std::cout << "Waiting before next connection attempt for " << wait_time << "\n";
                std::this_thread::sleep_for(wait_time);
            }
            // attempt connection
            auto conn = establish_connection(server_ip, port);
            last_connection_attempt_time = std::chrono::steady_clock::now();
            if (conn != nullptr) {
                is_connected.store(true);
                connection_retry_timeout = std::chrono::seconds(1); // reset retry timeout after successful connection
                std::cout << "Successfully established connection to server\n";
                return conn;
            } else {
                connection_retry_timeout = std::min(connection_retry_timeout * 2, max_retry_timeout);
            }
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

int start_of_connection(Connection* conn) {
    if (!conn) {
        return -1;
    }
    
    // send device name length and name
    uint32_t name_len = static_cast<uint32_t>(device_name.size());
    if (safe_SSL_write(conn, &name_len, sizeof(name_len)) < 0) {
        std::cerr << "Failed to send device name length\n";
        return -1;
    }
    if (safe_SSL_write(conn, device_name.c_str(), device_name.size()) < 0) {
        std::cerr << "Failed to send device name\n";
        return -1;
    }
    return 0;
}

int end_of_connection(Connection* conn) {
    if (!conn) return -1;
    return close_connection(conn);
}

int close_connection(Connection* conn) {
    if (!conn) return -1;

    auto shutdown_result = shutdown_ssl(conn->ssl);
    if (shutdown_result < 0) {
        std::cerr << "Failed to shutdown SSL connection properly\n";
    }

    SSL_free(conn->ssl);
    close(conn->sock);
    SSL_CTX_free(conn->ctx);
    delete conn;
    return shutdown_result;
}
    

int shutdown_ssl(SSL* ssl) {
    int shutdown_result;
    do {
        shutdown_result = SSL_shutdown(ssl);
        if (shutdown_result != 1 && shutdown_result != 0) {
            std::cerr << "SSL shutdown failed\n";
        }
    } while (shutdown_result == 0);
    return shutdown_result;
}