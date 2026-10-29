#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <iostream>

int main() {
    const int port = 8888;
    const int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::perror("socket");
        return 1;
    }

    int reuse = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::perror("setsockopt");
        close(listen_fd);
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(listen_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::perror("bind");
        close(listen_fd);
        return 1;
    }
    if (listen(listen_fd, 5) < 0) {
        std::perror("listen");
        close(listen_fd);
        return 1;
    }

    std::cout << "Echo server listening on port " << port << std::endl;
    for (;;) {
        // This first stage handles one connected client at a time.
        int client_fd = accept(listen_fd, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            std::perror("accept");
            break;
        }
        std::cout << "Client connected" << std::endl;

        char buffer[1024];
        for (;;) {
            ssize_t received = recv(client_fd, buffer, sizeof(buffer), 0);
            if (received == 0) {
                std::cout << "Client disconnected" << std::endl;
                break;
            }
            if (received < 0) {
                if (errno == EINTR) continue;
                std::perror("recv");
                break;
            }

            // send() can write fewer bytes than requested; send the remainder.
            ssize_t sent = 0;
            while (sent < received) {
                // MSG_NOSIGNAL 避免向断开的客户端发送时触发 SIGPIPE。
                ssize_t n = send(client_fd, buffer + sent, received - sent, MSG_NOSIGNAL);
                if (n < 0 && errno == EINTR) continue;
                if (n <= 0) {
                    if (n < 0) std::perror("send");
                    break;
                }
                sent += n;
            }
            if (sent != received) break;
        }
        close(client_fd);
    }
    close(listen_fd);
    return 1;
}
