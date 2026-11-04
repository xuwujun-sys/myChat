#include "TcpChannel.h"
#include "../handlers/Handler.h"
#include "../config/config.h"
#include <sys/socket.h>
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <utility>

TcpChannel::TcpChannel(int fd, std::function<bool(const TcpChannel&)> notifySend)
    : Channel(fd), notify_send_(std::move(notifySend)) {}

// 返回 false 表示发生错误，需要关闭连接。
bool TcpChannel::receiveData(Handler& handler)
{
    int fd = fd_;
    char buffer[4096];
    std::size_t received = 0;
    for (;;) {
        ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
        if (n > 0) {
            last_progress_ = std::chrono::steady_clock::now();
            if (!close_after_send_ && !handler.onData(*this, buffer, static_cast<size_t>(n))) return false;
            received += static_cast<std::size_t>(n);
            if (received >= 64 * 1024 || close_after_send_) return true;
        } else if (n == 0) {
            read_closed_ = true; // 对方不再发送，但可能还在等待剩余数据。
            return true;
        } else {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
            std::perror("recv");
            return false;
        }
    }
}

bool TcpChannel::queueSend(const char* data, std::size_t size)
{
    if (size > MAX_PENDING_BYTES - pending_.size()) {
        std::cerr << "Client send buffer limit exceeded: " << fd_ << std::endl;
        return false;
    }
    bool needsNotification = pending_.empty() && size > 0;
    pending_.append(data, size);
    if (needsNotification && notify_send_ && !notify_send_(*this)) {
        pending_.consume(size);
        return false;
    }
    return true;
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
            last_progress_ = std::chrono::steady_clock::now();
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

bool TcpChannel::isIdle(std::chrono::steady_clock::time_point now) const
{
    return now - last_progress_ >= std::chrono::seconds(CLIENT_IDLE_TIMEOUT_SECONDS);
}

