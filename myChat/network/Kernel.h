#pragma once
#include "TcpChannel.h"
#include "ListenChannel.h"
#include "Handler.h"
#include "../config/config.h"
#include <cstdint>
#include <memory>
#include <unordered_map>

// 管理 epoll 事件循环，并协调监听通道和客户端收发。
class Kernel {
public:
    // handler 的生命周期需要覆盖 Kernel。
    // 超时单位为秒；0 禁用空闲超时，负数会导致 run() 启动失败。
    explicit Kernel(Handler& handler, int idleTimeoutSeconds = CLIENT_IDLE_TIMEOUT_SECONDS)
        : handler_(handler), idle_timeout_seconds_(idleTimeoutSeconds) {}
    // 创建服务器并运行事件循环；启动失败或事件循环出错时返回非零值。
    int run();
private:
    int epoll_fd_ = -1; // Kernel 管理的 epoll 实例。
    ListenChannel listener_;
    Handler& handler_;
    int idle_timeout_seconds_;
    std::unordered_map<int, std::unique_ptr<TcpChannel>> clients_;
    // 接收并注册所有等待接入的客户端。
    void acceptConnections();
    // 处理一个客户端的读写事件和断开。
    void handleClientEvent(int fd, std::uint32_t flags);
    // 根据客户端状态修改 epoll 关注的读写事件；成功返回 true。
    bool updateEvents(int fd, const TcpChannel& channel);
    // 从 epoll 中移除客户端，关闭 fd，并删除它的状态。
    void closeClient(int fd);
};
