#pragma once

// 封装监听 socket 的创建和新客户端的接入。
class ListenChannel {
public:
    // 创建非阻塞监听 socket；成功返回监听 fd，失败返回 -1。
    static int open(int port);
    // 接收一个新连接；成功返回客户端 fd，失败返回 -1 并设置 errno。
    static int acceptClient(int listen_fd);
};
