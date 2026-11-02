#include "TcpChannel.h"
#include "config.h"
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <iostream>

TcpChannel::TcpChannel(int fd) : fd_(fd) {}

TcpChannel::~TcpChannel()
{
    if (fd_ >= 0) close(fd_);
}

// 返回 false 表示发生错误，需要关闭连接。
bool TcpChannel::receiveData()
{
    int fd = fd_;
    char buffer[4096];
    for (;;) {
        ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
        if (n > 0) {
            if (pending_.size() + static_cast<size_t>(n) > MAX_PENDING_BYTES) {
                std::cerr << "Client send buffer limit exceeded: " << fd << std::endl;
                return false;
            }
            pending_.append(buffer, static_cast<size_t>(n));
        } else if (n == 0) {
            read_closed_ = true; // 对方不再发送，但可能还在等回显。
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
bool TcpChannel::sendData()
{
    int fd = fd_;
    while (!pending_.empty()) {
        // MSG_NOSIGNAL 避免向已断开的客户端发送时触发 SIGPIPE。
        ssize_t n = send(fd, pending_.data(), pending_.size(), MSG_NOSIGNAL);
        if (n > 0) {
            // 标记已发送的数据，等待下次继续发送剩余部分。
            pending_.consume(static_cast<size_t>(n));
        } else {
            if (n < 0 && errno == EINTR) continue;
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;
            if (n < 0) std::perror("send");
            return false;
        }
    }
    return true;
}

