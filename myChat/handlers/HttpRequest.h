#pragma once
#include <cstddef>
#include <string>

// 保存解析后的 HTTP 请求信息。
struct HttpRequest {
    std::string method;
    std::string path;
    std::string host;
    std::size_t content_length = 0;
};
