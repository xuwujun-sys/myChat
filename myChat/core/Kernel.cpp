#include "Kernel.h"
#include "../config/config.h"
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
bool Kernel::updateEvents(int fd, const TcpChannel& channel)
{
    epoll_event event{};
    event.data.fd = fd;
    if (!channel.isReadClosed()) event.events |= EPOLLIN | EPOLLRDHUP;
    if (channel.hasPendingData()) event.events |= EPOLLOUT;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &event) == 0) return true;
    std::perror("epoll_ctl MOD client");
    return false;
}

void Kernel::closeClient(int fd)
{
   //从红黑树下摘除
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    auto it = clients_.find(fd);
    if (it != clients_.end()) {
        handler_.onDisconnected(*it->second);
        clients_.erase(it); // 销毁 TcpChannel，自动关闭客户端 fd。
    }
    std::cout << "Client disconnected: " << fd << std::endl;
}

void Kernel::acceptConnections()
{
    for (;;) {
        int client_fd = listener_.acceptClient();
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK) std::perror("accept4");
            break;
        }
        epoll_event client_event{};
        client_event.events = EPOLLIN | EPOLLRDHUP;
        client_event.data.fd = client_fd;
        // 注册通信的文件描述符到 epoll。
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &client_event) < 0) {
            std::perror("epoll_ctl ADD client");
            close(client_fd);
            continue;
        }
        auto notifySend = [this](const TcpChannel& channel) {
            return updateEvents(channel.getFd(), channel);
        };
        auto client = clients_.emplace(client_fd, std::unique_ptr<TcpChannel>(new TcpChannel(client_fd, notifySend)));
        handler_.onConnected(*client.first->second);
        std::cout << "Client connected: " << client_fd << std::endl;
    }
}

void Kernel::handleClientEvent(int fd, std::uint32_t flags)
{
    auto it = clients_.find(fd);
    if (it == clients_.end()) return;
    TcpChannel& channel = *it->second;

    // 按顺序处理：检查错误、接收、发送、更新事件或关闭连接。
    bool ok = !(flags & EPOLLERR);
    if (ok && (flags & (EPOLLIN | EPOLLRDHUP | EPOLLHUP))) {
        ok = channel.receiveData(handler_);
    }
    if (ok && channel.hasPendingData()) {
        ok = channel.sendData();
    }
    if (ok && !channel.hasPendingData() && !channel.shouldCloseAfterSend()) {
        ok = handler_.onSendComplete(channel);
    }
    // 根据当前收发状态更新关注的事件。
    if (ok && !((channel.isReadClosed() || channel.shouldCloseAfterSend()) && !channel.hasPendingData())) {
        ok = updateEvents(fd, channel);
    } else {
        ok = false;
    }
    if (!ok) closeClient(fd);
}

int Kernel::run()
{
    const int port = LISTEN_PORT;
    int listen_fd = listener_.open(port);
    if (listen_fd < 0) return 1;
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0) {
        std::perror("epoll_create1");
        listener_.closeListener();
        return 1;
    }
    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = listen_fd;
    //把listen_fd注册到epoll的红黑树上，本质上就是把文件描述符以及其需要epoll监听的事件注册一下
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd, &event) < 0) {
        std::perror("epoll_ctl ADD listener");
        close(epoll_fd_);
        epoll_fd_ = -1;
        listener_.closeListener();
        return 1;
    }

    epoll_event events[64];
    std::cout << "Server listening on port " << port << std::endl;
    for (;;) {
        //如果监听到文件描述符注册的事件了就返回，一次最多返回64个
        int count = epoll_wait(epoll_fd_, events, 64, -1);
        if (count < 0) {
            if (errno == EINTR) continue;
            std::perror("epoll_wait");
            break;
        }
        for (int i = 0; i < count; ++i) {
            int fd = events[i].data.fd;
            if (fd == listen_fd) {
                acceptConnections();
            } else {
                handleClientEvent(fd, events[i].events);
            }
        }
    }
    while (!clients_.empty()) closeClient(clients_.begin()->first);
    close(epoll_fd_);
    epoll_fd_ = -1;
    listener_.closeListener();
    return 1;
}
