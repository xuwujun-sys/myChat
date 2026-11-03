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
    bool onWritable(TcpChannel& channel) override;
    void onDisconnected(TcpChannel& channel) override;

private:
    struct RequestState {
        std::string header;
        std::string filename;
        std::string temporary;
        int directory_fd = -1;
        int file_fd = -1;
        std::size_t download_remaining = 0;
        HttpRequest request;
        std::size_t body_received = 0;
        bool header_complete = false;
    };
    std::unordered_map<int, RequestState> requests_;
    void cleanup(RequestState& state);
    bool startUpload(TcpChannel& channel, RequestState& state);
    bool writeUpload(TcpChannel& channel, RequestState& state, const char* data, std::size_t size);
    bool finishUpload(TcpChannel& channel, RequestState& state);
    bool startDownload(TcpChannel& channel, RequestState& state, const std::string& path);
};
