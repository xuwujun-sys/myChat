#pragma once
#include "TcpChannel.h"
#include <memory>
#include <unordered_map>

// 管理 epoll 事件循环，并协调监听通道和客户端收发。
class Kernel {
public:
    // 创建服务器并运行事件循环；启动失败或事件循环出错时返回非零值。
    int run();
private:
    int epoll_fd_ = -1; // Kernel 管理的 epoll 实例。
    // 根据客户端状态修改 epoll 关注的读写事件；成功返回 true。
    bool updateEvents(int fd, const TcpChannel& channel);
    // 从 epoll 中移除客户端，关闭 fd，并删除它的状态。
    void closeClient(int fd, std::unordered_map<int, std::unique_ptr<TcpChannel>>& clients);
};
