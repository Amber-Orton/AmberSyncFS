#include <iostream>
#include <fstream>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <thread>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "server.h"
#include <filesystem>
#include <sys/stat.h>
#include <send_recive_helper.h>
#include <database.h>
#include <send_recive.h>

int port = 0;


int main(int argc, char *argv[]) {
	if (argc != 4) {
		fprintf(stderr, "Usage: %s <port> <tracked_files_directory> <data_directory>\n", argv[0]);
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
    std::string tracked_files_directory = argv[2];
    if (!std::filesystem::exists(tracked_files_directory) || !std::filesystem::is_directory(tracked_files_directory)) {
        std::cerr << "Provided path is not a valid directory: " << tracked_files_directory << "\n";
        return 1;
    }

    // check data_dir and open database
    std::string data_dir = argv[3];
    if (!std::filesystem::exists(data_dir) || !std::filesystem::is_directory(data_dir)) {
        std::cerr << "Provided path is not a valid directory: " << data_dir << "\n";
        return 1;
    }
    if (!open_db(data_dir + "/deleted_file_times")) {
        std::cerr << "Failed to initialize server database at: " << data_dir + "/deleted_file_times" << "\n";
        return 1;
    }
    reset_in_progress_events(); // reset any events that were in progress

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
    if (server_fd < 0) {
        std::cerr << "Failed to create server socket: " << strerror(errno) << "\n";
        SSL_CTX_free(ctx);
        return 1;
    }

    int reuse_addr = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) < 0) {
        std::cerr << "Failed to set SO_REUSEADDR: " << strerror(errno) << "\n";
        close(server_fd);
        SSL_CTX_free(ctx);
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind server socket on port " << port << ": " << strerror(errno) << "\n";
        close(server_fd);
        SSL_CTX_free(ctx);
        return 1;
    }

    if (listen(server_fd, SOMAXCONN) < 0) {
        std::cerr << "Failed to listen on server socket: " << strerror(errno) << "\n";
        close(server_fd);
        SSL_CTX_free(ctx);
        return 1;
    }

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
        
        std::thread([tracked_files_directory, conn]() {
            // read client name length and name
            uint32_t client_name_len = 0;
            if (safe_SSL_read(conn, &client_name_len, sizeof(client_name_len)) <= 0) {
                std::cerr << "Failed to read client name length\n";
                close_connection(conn);
                return;
            }
            client_name_len = ntohl(client_name_len);
            std::vector<char> client_name_ch(client_name_len + 1);
            if (safe_SSL_read(conn, client_name_ch.data(), client_name_len) <= 0) {
                std::cerr << "Failed to read client name\n";
                close_connection(conn);
                return;
            }
            client_name_ch[client_name_len] = '\0'; // Null-terminate the client name
            std::cout << "Client connected with name: " << client_name_ch.data() << "\n";
            std::string client_name = std::string(client_name_ch.data());

            Event event;
            event.client_id = client_name;

            if (handle_incoming_event(conn, tracked_files_directory, &event) < 0) {
                std::cerr << "Error handling incoming event from client: " << client_name << "\n";
                close_connection(conn);
                return;
            } else {
                std::cout << "Successfully handled incoming event from client: " << client_name << "\n";
                end_of_connection(conn, event);
            }

        }).detach();
    }
    SSL_CTX_free(ctx);
    close(server_fd);
    return 0;
}

void end_of_connection(Connection* conn, Event& event) {
    close_connection(conn);

    // create events for all other clients to process the changes from this client
    auto origional_client_id = event.client_id;
    for (std::string user : get_all_users()) {
        if (user != origional_client_id) {
            event.client_id = user;
            create_event(event);
        }
    }
}

void close_connection(Connection* conn) {
    shutdown_ssl(conn->ssl);
    SSL_free(conn->ssl);
    close(conn->sock);
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