#pragma once
#include "Protocol.h"

// 解析和校验 HTTP 请求头。
class HttpProtocol : public Protocol {
public:
    int parseHeader(const std::string& header, HttpRequest& request) const override;
};
