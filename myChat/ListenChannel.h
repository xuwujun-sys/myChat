#pragma once
#include "Channel.h"

// 封装监听 socket 的创建和新客户端的接入。
class ListenChannel : public Channel {
public:
    ListenChannel() = default;
    ~ListenChannel() override = default;

    // 创建并保存非阻塞监听 socket；成功返回监听 fd，失败返回 -1。
    int open(int port);
    // 关闭监听 socket，可以重复调用。
    void closeListener();
    // 接收一个新连接；成功返回客户端 fd，失败返回 -1 并设置 errno。
    int acceptClient() const;

};
