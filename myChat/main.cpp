#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <iostream>
#include <string>
#include <unordered_map>

//当前阶段使用水平触发（LT）和非阻塞 socket，这样的话recv和send都是非阻塞的了
//ET与非阻塞的通信描述符搭配使用更多，LT和ET的最大的区别大概就是LT只要就绪就通知，ET只通知一次
//所谓的就绪就是你注册的epoll的事件就绪比如监听的文件描述符注册EPOLLIN就是对端有连接
/*
EPOLLIN：Socket 内核接收缓冲区有数据可读（或者对端关闭了连接）。
EPOLLOUT：Socket 内核发送缓冲区有可用空间，可以继续发送数据（一般都是有空间的）。
*/
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
/*
由于单个通信fd的写缓冲区是有限制的所以不一定可以一次发完，如果一次可以发完那就发完了，如果没有发完的话，
那就先发送缓冲区大小的数据，然后下次再发送就会异常并且设置errno = EAGAIN/errno = EWOULDBLOCK，
当然这也是因为设置的非阻塞，不然会阻塞的等。
*/
bool sendData(int fd, Client& client)
{
    while (!client.pending.empty()) {
        // MSG_NOSIGNAL 避免向已断开的客户端发送时触发 SIGPIPE。
        ssize_t n = send(fd, client.pending.data(), client.pending.size(), MSG_NOSIGNAL);
        if (n > 0) {
            //擦除已经发送的部分，等待下次继续发送
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
   //从红黑树下摘除
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    clients.erase(fd);
    std::cout << "Client disconnected: " << fd << std::endl;
}

int main()
{
    const int port = 8888;
    //SOCK_NONBLOCK | SOCK_CLOEXEC分别是把listen_fd设置成非阻塞和当使用exe执行系列的函数时不会对外泄露fd，他们都是这个文件描述符的属性
    //这里注册成非阻塞的话后续accept就是非阻塞的，如果没有对端没有连接不会死等而是返回负数并且设置errorn
    int listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listen_fd < 0) {
        std::perror("socket");
        return 1;
    }
    int reuse = 1;
    //这是设置listen_fd的一个选项表述可以地址端口复用，对应tcp的timewait机制，使得当前服务器退出之后立马重启还能连上
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
                    int client_fd = accept4(listen_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
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
                ok = receiveData(fd, client);
            }
            if (ok && !client.pending.empty()) {
                ok = sendData(fd, client);
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
