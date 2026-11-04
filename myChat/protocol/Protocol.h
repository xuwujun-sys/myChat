#pragma once
#include <string>

struct HttpRequest;

// 当前协议接口：解析完整请求头，返回 0 表示成功，否则返回错误状态码。
class Protocol {
public:
    virtual ~Protocol() = default;
    virtual int parseHeader(const std::string& header, HttpRequest& request) const = 0;
};
