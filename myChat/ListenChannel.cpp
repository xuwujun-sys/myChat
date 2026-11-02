#include "ListenChannel.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>

int ListenChannel::open(int port)
{
    //SOCK_NONBLOCK | SOCK_CLOEXEC分别是把listen_fd设置成非阻塞和当使用exe执行系列的函数时不会对外泄露fd，他们都是这个文件描述符的属性
    //这里注册成非阻塞的话后续accept就是非阻塞的，如果没有对端没有连接不会死等而是返回负数并且设置errorn
    int listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listen_fd < 0) {
        std::perror("socket");
        return -1;
    }
    int reuse = 1;
    //这是设置listen_fd的一个选项表述可以地址端口复用，对应tcp的timewait机制，使得当前服务器退出之后立马重启还能连上
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::perror("setsockopt");
        close(listen_fd);
        return -1;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(listen_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        listen(listen_fd, 128) < 0) {
        std::perror("bind/listen");
        close(listen_fd);
        return -1;
    }
    return listen_fd;
}

int ListenChannel::acceptClient(int listen_fd)
{
    return accept4(listen_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
}
