#pragma once
#include "Handler.h"
#include <string>
#include <unordered_map>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string host;
    std::size_t content_length = 0;
};

// 每个连接接收一个 HTTP 请求，支持小文件上传和下载。
class HttpHandler : public Handler {
public:
    void onConnected(TcpChannel& channel) override;
    bool onData(TcpChannel& channel, const char* data, std::size_t size) override;
    void onDisconnected(TcpChannel& channel) override;

private:
    struct RequestState {
        std::string header;
        std::string body; // 小文件上传时保存请求体。
        std::string filename;
        HttpRequest request;
        std::size_t body_received = 0;
        bool header_complete = false;
    };
    std::unordered_map<int, RequestState> requests_;
};
