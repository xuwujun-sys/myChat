#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <iostream>
#include <string>
#include <unordered_map>

// 当前阶段使用水平触发（LT）和非阻塞 socket。
struct Client {
    std::string pending; // 等待回传的数据
    bool read_closed = false;
};

// 返回 false 表示发生错误，需要关闭连接。
bool receiveData(int fd, Client& client)
{
    const size_t max_pending = 1024 * 1024;
    char buffer[4096];
    for (;;) {
        ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
        if (n > 0) {
            if (client.pending.size() + static_cast<size_t>(n) > max_pending) {
                std::cerr << "Client send buffer limit exceeded: " << fd << std::endl;
                return false;
            }
            client.pending.append(buffer, static_cast<size_t>(n));
        } else if (n == 0) {
            client.read_closed = true; // 对方不再发送，但可能还在等回显。
            return true;
        } else {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
            std::perror("recv");
            return false;
        }
    }
}

bool sendData(int fd, Client& client)
{
    while (!client.pending.empty()) {
        // MSG_NOSIGNAL 避免向已断开的客户端发送时触发 SIGPIPE。
        ssize_t n = send(fd, client.pending.data(), client.pending.size(), MSG_NOSIGNAL);
        if (n > 0) {
            client.pending.erase(0, static_cast<size_t>(n));
        } else {
            if (n < 0 && errno == EINTR) continue;
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;
            if (n < 0) std::perror("send");
            return false;
        }
    }
    return true;
}

bool updateEvents(int epoll_fd, int fd, const Client& client)
{
    epoll_event event{};
    event.data.fd = fd;
    if (!client.read_closed) event.events |= EPOLLIN | EPOLLRDHUP;
    if (!client.pending.empty()) event.events |= EPOLLOUT;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) == 0) return true;
    std::perror("epoll_ctl MOD client");
    return false;
}

void closeClient(int epoll_fd, int fd, std::unordered_map<int, Client>& clients)
{
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    clients.erase(fd);
    std::cout << "Client disconnected: " << fd << std::endl;
}

int main()
{
    const int port = 8888;
    int listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
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
    if (bind(listen_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        listen(listen_fd, 128) < 0) {
        std::perror("bind/listen");
        close(listen_fd);
        return 1;
    }
    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0) {
        std::perror("epoll_create1");
        close(listen_fd);
        return 1;
    }
    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event) < 0) {
        std::perror("epoll_ctl ADD listener");
        close(epoll_fd);
        close(listen_fd);
        return 1;
    }

    std::unordered_map<int, Client> clients;
    epoll_event events[64];
    std::cout << "Echo server listening on port " << port << std::endl;
    for (;;) {
        int count = epoll_wait(epoll_fd, events, 64, -1);
        if (count < 0) {
            if (errno == EINTR) continue;
            std::perror("epoll_wait");
            break;
        }
        for (int i = 0; i < count; ++i) {
            int fd = events[i].data.fd;
            uint32_t flags = events[i].events;
            if (fd == listen_fd) {
                for (;;) {
                    int client_fd = accept4(listen_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
                    if (client_fd < 0) {
                        if (errno == EINTR) continue;
                        if (errno != EAGAIN && errno != EWOULDBLOCK) std::perror("accept4");
                        break;
                    }
                    epoll_event client_event{};
                    client_event.events = EPOLLIN | EPOLLRDHUP;
                    client_event.data.fd = client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) < 0) {
                        std::perror("epoll_ctl ADD client");
                        close(client_fd);
                        continue;
                    }
                    clients.emplace(client_fd, Client{});
                    std::cout << "Client connected: " << client_fd << std::endl;
                }
                continue;
            }

            auto it = clients.find(fd);
            if (it == clients.end()) continue;
            Client& client = it->second;

            // 按顺序处理：检查错误、接收、发送、更新事件或关闭连接。
            bool ok = !(flags & EPOLLERR);
            if (ok && (flags & (EPOLLIN | EPOLLRDHUP | EPOLLHUP))) {
                ok = receiveData(fd, client);
            }
            if (ok && !client.pending.empty()) {
                ok = sendData(fd, client);
            }
            if (ok && !(client.read_closed && client.pending.empty())) {
                ok = updateEvents(epoll_fd, fd, client);
            } else {
                ok = false;
            }
            if (!ok) closeClient(epoll_fd, fd, clients);
        }
    }
    for (const auto& entry : clients) close(entry.first);
    close(epoll_fd);
    close(listen_fd);
    return 1;
}
