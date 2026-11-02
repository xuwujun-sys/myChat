#include "Kernel.h"
#include "ListenChannel.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <iostream>

//当前阶段使用水平触发（LT）和非阻塞 socket，这样的话recv和send都是非阻塞的了
//ET与非阻塞的通信描述符搭配使用更多，LT和ET的最大的区别大概就是LT只要就绪就通知，ET只通知一次
//所谓的就绪就是你注册的epoll的事件就绪比如监听的文件描述符注册EPOLLIN就是对端有连接
/*
EPOLLIN：Socket 内核接收缓冲区有数据可读（或者对端关闭了连接）。
EPOLLOUT：Socket 内核发送缓冲区有可用空间，可以继续发送数据（一般都是有空间的）。
*/
bool Kernel::updateEvents(int epoll_fd, int fd, const Client& client)
{
    epoll_event event{};
    event.data.fd = fd;
    if (!client.read_closed) event.events |= EPOLLIN | EPOLLRDHUP;
    if (!client.pending.empty()) event.events |= EPOLLOUT;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) == 0) return true;
    std::perror("epoll_ctl MOD client");
    return false;
}

void Kernel::closeClient(int epoll_fd, int fd, std::unordered_map<int, Client>& clients)
{
   //从红黑树下摘除
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    clients.erase(fd);
    std::cout << "Client disconnected: " << fd << std::endl;
}

int Kernel::run()
{
    const int port = 8888;
    int listen_fd = ListenChannel::open(port);
    if (listen_fd < 0) return 1;
    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0) {
        std::perror("epoll_create1");
        close(listen_fd);
        return 1;
    }
    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = listen_fd;
    //把listen_fd注册到epoll的红黑树上，本质上就是把文件描述符以及其需要epoll监听的事件注册一下
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
        //如果监听到文件描述符注册的事件了就返回，一次最多返回64个
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
                    int client_fd = ListenChannel::acceptClient(listen_fd);
                    if (client_fd < 0) {
                        if (errno == EINTR) continue;
                        if (errno != EAGAIN && errno != EWOULDBLOCK) std::perror("accept4");
                        break;
                    }
                    epoll_event client_event{};
                    client_event.events = EPOLLIN | EPOLLRDHUP;
                    client_event.data.fd = client_fd;
                    //注册通信的文件描述符到epoll
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
                ok = TcpChannel::receiveData(fd, client);
            }
            if (ok && !client.pending.empty()) {
                ok = TcpChannel::sendData(fd, client);
            }
            //由于前面的收发当前通信文件描述符需要监听的事件有所变化需要跟新
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
