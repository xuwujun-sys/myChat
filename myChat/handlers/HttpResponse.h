#pragma once
#include <string>

// 生成当前服务器使用的固定响应和错误响应。
class HttpResponse {
public:
    explicit HttpResponse(int status);
    // 生成状态行和响应头。
    std::string buildHeader() const;
    // 生成完整响应（响应头和响应体）。
    std::string buildMessage() const;

private:
    int status_code_ = 200;
    std::string reason_ = "OK";
    std::string content_type_;
    std::string body_;
};
