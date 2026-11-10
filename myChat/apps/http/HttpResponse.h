#pragma once
#include <string>
#include <cstddef>

// 生成当前服务器使用的固定响应和错误响应。
class HttpResponse {
public:
    explicit HttpResponse(int status);
    // 自定义内容类型和长度，适用于单独发送响应头的场景。
    HttpResponse(int status, const std::string& content_type, std::size_t content_length);
    // 生成状态行和响应头。
    std::string buildHeader() const;
    // 生成完整响应（响应头和响应体）。
    std::string buildMessage() const;

private:
    int status_code_ = 200;
    std::string reason_ = "OK";
    std::string content_type_;
    std::string body_;
    std::size_t content_length_ = 0;
};
