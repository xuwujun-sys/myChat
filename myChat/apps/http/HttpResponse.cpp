#include "HttpResponse.h"

HttpResponse::HttpResponse(int status)
{
    switch (status) {
    case 201: status_code_ = 201; reason_ = "Created"; break;
    case 400: status_code_ = 400; reason_ = "Bad Request"; break;
    case 404: status_code_ = 404; reason_ = "Not Found"; break;
    case 409: status_code_ = 409; reason_ = "Conflict"; break;
    case 411: status_code_ = 411; reason_ = "Length Required"; break;
    case 413: status_code_ = 413; reason_ = "Content Too Large"; break;
    case 431: status_code_ = 431; reason_ = "Request Header Fields Too Large"; break;
    case 500: status_code_ = 500; reason_ = "Internal Server Error"; break;
    case 501: status_code_ = 501; reason_ = "Not Implemented"; break;
    default:
        content_type_ = "text/plain; charset=utf-8";
        body_ = "Hello, HTTP!\n";
        break;
    }
    content_length_ = body_.size();
}

HttpResponse::HttpResponse(int status, const std::string& content_type, std::size_t content_length)
    : HttpResponse(status)
{
    content_type_ = content_type;
    content_length_ = content_length;
    body_.clear();
}

std::string HttpResponse::buildHeader() const
{
    std::string header = "HTTP/1.1 " + std::to_string(status_code_) + " " + reason_ + "\r\n";
    if (!content_type_.empty()) header += "Content-Type: " + content_type_ + "\r\n";
    header += "Content-Length: " + std::to_string(content_length_) + "\r\nConnection: close\r\n\r\n";
    return header;
}

std::string HttpResponse::buildMessage() const
{
    return buildHeader() + body_;
}
