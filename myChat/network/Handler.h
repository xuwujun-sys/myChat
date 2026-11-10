#pragma once
#include <cstddef>

class TcpChannel;

// 处理客户端连接及接收到的数据。
class Handler {
public:
    virtual ~Handler() = default;

    // 客户端连接建立后调用。
    virtual void onConnected(TcpChannel& channel) = 0;

    // 收到一段字节流时调用；返回 false 表示处理失败。
    // data 仅在本次调用期间有效，字节流可能包含不完整或多条消息。
    virtual bool onData(TcpChannel& channel, const char* data, std::size_t size) = 0;

    // 发送缓冲清空后通知处理器，可以继续发送数据。
    virtual bool onSendComplete(TcpChannel&) { return true; }

    // 客户端连接断开时调用。
    virtual void onDisconnected(TcpChannel& channel) = 0;
};
