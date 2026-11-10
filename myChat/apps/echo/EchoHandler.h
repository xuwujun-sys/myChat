#pragma once
#include "../../network/Handler.h"

// 将收到的数据原样放入客户端的发送缓冲区。
class EchoHandler : public Handler {
public:
    void onConnected(TcpChannel& channel) override;
    bool onData(TcpChannel& channel, const char* data, std::size_t size) override;
    void onDisconnected(TcpChannel& channel) override;
};
