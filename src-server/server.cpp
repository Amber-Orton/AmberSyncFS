
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "server.h"
#include <filesystem>
#include <sys/stat.h>
#include "../src-common/send_recive.h"

int port = 0;


int main(int argc, char *argv[]) {
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <port> <directory>\n", argv[0]);
		return 1;
	}

    // collect the port and directory to be synced to
    // and check that they exist and are valid
    port = std::stoi(argv[1]);
    if (port <= 0 || port > 65535) {
        std::cerr << "Invalid port number: " << port << "\n";
        return 1;
    }
    // and check that it exists and is a directory
    std::string directory = argv[2];
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        std::cerr << "Provided path is not a valid directory: " << directory << "\n";
        return 1;
    }

    // 1. Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        std::cerr << "Failed to create SSL_CTX\n";
        return 1;
    }
    if (SSL_CTX_use_certificate_file(ctx, "certs/server.crt", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, "certs/server.key", SSL_FILETYPE_PEM) <= 0) {
        std::cerr << "Failed to load cert/key\n";
        SSL_CTX_free(ctx);
        return 1;
    }

    // Load CA or client certificate for client verification
    if (SSL_CTX_load_verify_locations(ctx, "certs/ca.crt", nullptr) != 1) {
        std::cerr << "Failed to load CA/client certificate for client verification\n";
        SSL_CTX_free(ctx);
        return 1;
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);

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
        if (client_fd < 0) {
            std::cerr << "Failed to accept client connection\n";
            continue;
        }
        
        SSL* ssl = SSL_new(ctx);
        SSL_set_fd(ssl, client_fd);
        if (SSL_accept(ssl) <= 0) {
            std::cerr << "SSL handshake failed\n";
            SSL_free(ssl);
            close(client_fd);
            continue;
        }
        Connection* conn = new Connection();
        conn->ctx = ctx;
        conn->ssl = ssl;
        conn->sock = client_fd;
        
        // Verify client certificate
        long verify_result = SSL_get_verify_result(ssl);
        if (verify_result != X509_V_OK) {
            std::cerr << "Client certificate verification failed: " << X509_verify_cert_error_string(verify_result) << "\n";
            shutdown_ssl(ssl);
            SSL_free(ssl);
            close(client_fd);
            continue;
        }


        // Read command from client
        char command[2];
        int bytes_read = safe_SSL_read(conn, command, sizeof(command)); // Read command type (e.g., "UP" for upload)
        if (bytes_read <= 0) {
            std::cerr << "Failed to read command from client\n";
            close_connection(ssl, client_fd);
            continue;
        }


        // Handle command, its the programmers responsibility to ensure that commands are all distinct and of a fixed length
        // each in a thread, allows infinite clients to connect and send commands without blocking each other
        // means can be DOSed by opening many connections and sending commands without closing them but clients are certified and assumed to be non-malicious
        if (std::strncmp(command, "UF", 2) == 0) {
            std::cout << "Received upload command from client\n";
            std::thread([directory, conn]() {
                if (receive_file_tls(directory, conn) < 0) {
                    std::cerr << "Failed to receive file\n";
                    close_connection(conn);
                } else {
                    end_of_connection(conn);
                }
            }).detach();
        } else if (std::strncmp(command, "DF", 2) == 0) {
            std::cout << "Received delete command from client\n";
            std::thread([directory, conn]() {
                if (receive_delete_file_tls(directory, conn) < 0) {
                    std::cerr << "Failed to delete file\n";
                    close_connection(conn);
                } else {
                    end_of_connection(conn);
                }
            }).detach();
        } else if (std::strncmp(command, "UD", 2) == 0) {
            std::cout << "Received upload directory command from client\n";
            std::thread([directory, conn]() {
                if (receive_directory_tls(directory, conn) < 0) {
                    std::cerr << "Failed to receive directory\n";
                    close_connection(conn);
                } else {
                    end_of_connection(conn);
                }
            }).detach();
        } else if (std::strncmp(command, "DD", 2) == 0) {
            std::cout << "Received delete directory command from client\n";
            std::thread([directory, conn]() {
                if (receive_delete_directory_tls(directory, conn) < 0) {
                    std::cerr << "Failed to delete directory\n";
                    close_connection(conn);
                } else {
                    end_of_connection(conn);
                }
            }).detach();
        } else {
            std::cerr << "Unknown command received from client: " << std::string(command, 2) << "\n";
            close_connection(conn);
            continue;
        }
    }
    SSL_CTX_free(ctx);
    close(server_fd);
    return 0;
}

void end_of_connection(Connection* conn) {
    close_connection(conn);
}

void close_connection(Connection* conn) {
    shutdown_ssl(conn->ssl);
    SSL_free(conn->ssl);
    close(conn->sock);
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