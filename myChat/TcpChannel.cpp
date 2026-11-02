#include "TcpChannel.h"
#include <sys/socket.h>
#include <cerrno>
#include <cstdio>
#include <iostream>

// 返回 false 表示发生错误，需要关闭连接。
bool TcpChannel::receiveData(int fd, Client& client)
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
bool TcpChannel::sendData(int fd, Client& client)
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

