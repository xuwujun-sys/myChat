#pragma once
#include "Handler.h"

// 返回固定的 HTTP 响应，后续再加入请求解析。
class HttpHandler : public Handler {
public:
    void onConnected(TcpChannel& channel) override;
    bool onData(TcpChannel& channel, const char* data, std::size_t size) override;
    void onDisconnected(TcpChannel& channel) override;
};
