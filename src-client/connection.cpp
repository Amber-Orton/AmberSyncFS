#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

bool send_file(const char* server_ip, int port, const char* file_path) {
    // 1. Open file
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file\n";
        return false;
    }

    // 2. Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation failed\n";
        return false;
    }

    // 3. Set up server address
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    // 4. Connect
    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed\n";
        close(sock);
        return false;
    }

    // 5. Send file name length and name
    std::string filename = file_path;
    size_t pos = filename.find_last_of("/\\");
    std::string just_name = (pos == std::string::npos) ? filename : filename.substr(pos + 1);
    uint32_t name_len = htonl(just_name.size());
    send(sock, &name_len, sizeof(name_len), 0);
    send(sock, just_name.c_str(), just_name.size(), 0);

    // 6. Send file data
    char buffer[4096];
    while (file) {
        file.read(buffer, sizeof(buffer));
        std::streamsize bytes = file.gcount();
        if (bytes > 0) {
            send(sock, buffer, bytes, 0);
        }
    }

    // 7. Clean up
    close(sock);
    file.close();
    std::cout << "File sent!\n";
    return true;
}