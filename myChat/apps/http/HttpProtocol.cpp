#include "HttpProtocol.h"
#include "HttpRequest.h"
#include <cctype>
#include <limits>

namespace {
// 判断字符是否可以用于 HTTP 方法或请求头名称。
bool isTokenChar(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || std::string("!#$%&'*+-.^_`|~").find(c) != std::string::npos;
}

// 去除请求头值两端的空格和制表符。
std::string trim(const std::string& value)
{
    std::size_t start = value.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    return value.substr(start, value.find_last_not_of(" \t") - start + 1);
}

}

// 解析并校验请求行和请求头；返回 0 表示有效，否则返回错误状态码。
int HttpProtocol::parseHeader(const std::string& header, HttpRequest& request) const
{
    std::size_t end = header.find("\r\n");
    if (end == std::string::npos) return 400;
    std::string line = header.substr(0, end);
    std::size_t first = line.find(' ');
    std::size_t second = first == std::string::npos ? first : line.find(' ', first + 1);
    if (first == 0 || first == std::string::npos || second == std::string::npos ||
        second == first + 1 || line.find(' ', second + 1) != std::string::npos ||
        line.substr(second + 1) != "HTTP/1.1") return 400;
    request.method = line.substr(0, first);
    request.path = line.substr(first + 1, second - first - 1);
    for (unsigned char c : request.method) if (!isTokenChar(c)) return 400;
    if (request.path.empty() || request.path[0] != '/') return 400;
    for (unsigned char c : request.path) if (c <= 32 || c == 127) return 400;

    bool hasHost = false;
    bool hasLength = false;
    bool hasTransferEncoding = false;
    bool hasContentEncoding = false;
    std::size_t pos = end + 2;
    while (pos < header.size()) {
        end = header.find("\r\n", pos);
        if (end == std::string::npos) return 400;
        if (end == pos) break;
        line = header.substr(pos, end - pos);
        std::size_t colon = line.find(':');
        if (colon == 0 || colon == std::string::npos) return 400;
        std::string name = line.substr(0, colon);
        for (char& c : name) {
            if (!isTokenChar(static_cast<unsigned char>(c))) return 400;
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        std::string value = trim(line.substr(colon + 1));
        for (unsigned char c : value) if ((c < 32 && c != '\t') || c == 127) return 400;
        if (name == "host") {
            if (hasHost || value.empty() || value.find_first_of(" \t,") != std::string::npos) return 400;
            hasHost = true;
            request.host = value;
        } else if (name == "content-length") {
            if (hasLength || value.empty()) return 400;
            hasLength = true;
            for (unsigned char c : value) {
                if (c < '0' || c > '9') return 400;
                std::size_t digit = c - '0';
                if (request.content_length > (std::numeric_limits<std::size_t>::max() - digit) / 10) return 400;
                request.content_length = request.content_length * 10 + digit;
            }
        } else if (name == "transfer-encoding") {
            hasTransferEncoding = true;
        } else if (name == "content-encoding") {
            hasContentEncoding = true;
        }
        pos = end + 2;
    }
    if (!hasHost || (hasTransferEncoding && hasLength)) return 400;
    if (hasTransferEncoding || hasContentEncoding) return 501;
    if (request.method == "PUT" && !hasLength) return 411;
    return 0;
}
