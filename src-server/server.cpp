
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "server.h"
#include <filesystem>

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
		return 1;
	}

    // collect the directory to be synced to
    // and check that it exists and is a directory
    const char* directory = argv[1];
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        std::cerr << "Provided path is not a valid directory: " << directory << "\n";
        return 1;
    }


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
        int bytes_read = SSL_read(ssl, command, sizeof(command)); // Read command type (e.g., "UP" for upload)
        if (bytes_read <= 0) {
            std::cerr << "Failed to read command from client\n";
            shutdown_ssl(ssl);
            SSL_free(ssl);
            close(client_fd);
            continue;
        }


        // Handle command its the programmers responsibility to ensure that commands are all distinct and of a fixed length
        if (std::strncmp(command, "UP", 2) == 0) {
             std::cout << "Received upload command from client\n";
            if(receive_file(ssl, directory) < 0) {
                std::cerr << "Failed to receive file\n";
                shutdown_ssl(ssl);
                SSL_free(ssl);
                close(client_fd);
                continue;
            }
        }



        shutdown_ssl(ssl);
        SSL_free(ssl);
        close(client_fd);
    }
    SSL_CTX_free(ctx);
    close(server_fd);
    return 0;
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

int receive_file(SSL* ssl, const char* directory) {
    uint32_t name_len = 0;
    int n1 = SSL_read(ssl, &name_len, sizeof(name_len));
    std::cout << "[DEBUG] Read file name length: " << n1 << " bytes\n" << std::flush;
    name_len = ntohl(name_len);
    if (name_len == 0 || name_len > 256) {
        std::cerr << "Invalid filename length: " << name_len << "\n" << std::flush;
        return -1;
    }

    char filename[257] = {0};
    int n2 = SSL_read(ssl, filename, name_len);
    filename[name_len] = '\0';
    std::cout << "[DEBUG] Read file name: '" << filename << "' (" << n2 << " bytes)\n" << std::flush;

    std::ofstream outfile(std::filesystem::path(directory) / filename, std::ios::binary);
    if (!outfile) {
        std::cerr << "Failed to open file for writing: " << filename << "\n" << std::flush;
        return -1;
    }
    std::cout << "Receiving file: '" << filename << "'..." << std::endl << std::flush;
    char buffer[4096];
    int bytes;
    size_t total_bytes = 0;
    while ((bytes = SSL_read(ssl, buffer, sizeof(buffer))) > 0) {
        outfile.write(buffer, bytes);
        total_bytes += bytes;
    }
    std::cout << "Received file: '" << filename << "' (" << total_bytes << " bytes)" << std::endl << std::flush;
    
    outfile.flush();
    outfile.close();
    return 0;
}