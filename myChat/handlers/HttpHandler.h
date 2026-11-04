#pragma once
#include "Handler.h"
#include "HttpRequest.h"
#include "../core/UniqueFd.h"
#include <string>
#include <unordered_map>

// 每个连接接收一个 HTTP 请求，支持小文件上传和下载。
class HttpHandler : public Handler {
public:
    void onConnected(TcpChannel& channel) override;
    bool onData(TcpChannel& channel, const char* data, std::size_t size) override;
    bool onSendComplete(TcpChannel& channel) override;
    void onDisconnected(TcpChannel& channel) override;

private:
    struct RequestState {
        std::string header;
        std::string body; // 小文件上传时保存请求体。
        std::string filename;
        UniqueFd download_file;
        std::size_t download_remaining = 0;
        HttpRequest request;
        std::size_t body_received = 0;
        bool header_complete = false;
    };
    std::unordered_map<int, RequestState> requests_;
    bool startDownload(TcpChannel& channel, RequestState& state, const std::string& path);
};
