#include "HttpHandler.h"
#include "HttpResponse.h"
#include "../core/TcpChannel.h"
#include "../config/config.h"
#include <cctype>
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>

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

// 解析并校验请求行和请求头；返回 0 表示有效，否则返回错误状态码。
int parseHeader(const std::string& header, HttpRequest& request)
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

// 按状态码生成响应，发送完成后关闭连接。
bool respond(TcpChannel& channel, int status)
{
    HttpResponse response(status);
    std::string message = response.buildMessage();
    if (!channel.queueSend(message.data(), message.size())) return false;
    channel.closeAfterSend();
    return true;
}

// 将 URL 中的文件名解码，并限制为单个安全的路径分量。
bool decodeFilename(const std::string& path, std::string& filename)
{
    for (std::size_t i = 0; i < path.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(path[i]);
        if (c == '%') {
            if (i + 2 >= path.size()) return false;
            int value = 0;
            for (int j = 0; j < 2; ++j) {
                unsigned char hex = static_cast<unsigned char>(path[++i]);
                if (hex >= '0' && hex <= '9') value = value * 16 + hex - '0';
                else if (hex >= 'a' && hex <= 'f') value = value * 16 + hex - 'a' + 10;
                else if (hex >= 'A' && hex <= 'F') value = value * 16 + hex - 'A' + 10;
                else return false;
            }
            c = static_cast<unsigned char>(value);
        } else if (c == '?' || c == '#') {
            return false;
        }
        if (c < 32 || c == 127 || c == '/' || c == '\\') return false;
        filename.push_back(static_cast<char>(c));
    }
    return !filename.empty() && filename != "." && filename != "..";
}

// 读取文件目录中的小文件，并发送包含长度的 HTTP 响应。
bool sendFile(TcpChannel& channel, const std::string& path)
{
    std::string filename;
    if (!decodeFilename(path, filename)) return respond(channel, 400);
    int directory = open(HTTP_FILES_DIR, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory < 0) return respond(channel, 500);
    int file = openat(directory, filename.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    int openError = errno;
    close(directory);
    if (file < 0) return respond(channel, openError == ENOENT || openError == ELOOP ? 404 : 500);

    struct stat info{};
    if (fstat(file, &info) < 0) {
        close(file);
        return respond(channel, 500);
    }
    if (!S_ISREG(info.st_mode)) {
        close(file);
        return respond(channel, 404);
    }
    if (info.st_size < 0 || static_cast<unsigned long long>(info.st_size) > MAX_PENDING_BYTES) {
        close(file);
        return respond(channel, 413);
    }
    std::size_t size = static_cast<std::size_t>(info.st_size);
    std::string response = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " +
        std::to_string(size) + "\r\nConnection: close\r\n\r\n";
    if (size > MAX_PENDING_BYTES - response.size()) {
        close(file);
        return respond(channel, 413);
    }
    char buffer[4096];
    std::size_t remaining = size;
    while (remaining > 0) {
        std::size_t amount = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        ssize_t count = read(file, buffer, amount);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            close(file);
            return respond(channel, 500);
        }
        response.append(buffer, static_cast<std::size_t>(count));
        remaining -= static_cast<std::size_t>(count);
    }
    close(file);
    if (!channel.queueSend(response.data(), response.size())) return false;
    channel.closeAfterSend();
    return true;
}

// 将完整请求体写入临时文件，再创建目标文件；不覆盖已有文件。
bool saveFile(TcpChannel& channel, const std::string& filename, const std::string& body)
{
    int directory = open(HTTP_FILES_DIR, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory < 0) return respond(channel, 500);

    static unsigned long nextId = 0;
    std::string temporary;
    int file = -1;
    for (int attempts = 0; attempts < 100; ++attempts) {
        temporary = ".upload-" + std::to_string(getpid()) + "-" + std::to_string(nextId++);
        // 临时文件不能和客户端请求的目标文件同名。
        if (temporary == filename) continue;
        file = openat(directory, temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (file >= 0 || errno != EEXIST) break;
    }
    if (file < 0) {
        close(directory);
        return respond(channel, 500);
    }
    std::size_t written = 0;
    while (written < body.size()) {
        ssize_t count = write(file, body.data() + written, body.size() - written);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        written += static_cast<std::size_t>(count);
    }
    bool ok = written == body.size();
    if (close(file) < 0) ok = false;
    int status = 201;
    if (!ok) status = 500;
    else if (linkat(directory, temporary.c_str(), directory, filename.c_str(), 0) < 0) {
        status = errno == EEXIST ? 409 : 500;
    }
    unlinkat(directory, temporary.c_str(), 0);
    close(directory);
    return respond(channel, status);
}
}

// 客户端连接时创建独立的请求状态。
void HttpHandler::onConnected(TcpChannel& channel)
{
    requests_.emplace(channel.getFd(), RequestState{});
}

// 累积并解析收到的数据，在请求完整或出错时返回响应。
bool HttpHandler::onData(TcpChannel& channel, const char* data, std::size_t size)
{
    RequestState& state = requests_.at(channel.getFd());
    if (!state.header_complete) {
        state.header.append(data, size);
        std::size_t end = state.header.find("\r\n\r\n");
        if (end == std::string::npos) {
            if (state.header.size() > MAX_HTTP_HEADER_BYTES) return respond(channel, 431);
            return true;
        }
        std::size_t headerSize = end + 4;
        if (headerSize > MAX_HTTP_HEADER_BYTES) return respond(channel, 431);
        int status = parseHeader(state.header.substr(0, headerSize), state.request);

        if (status != 0) return respond(channel, status);
        const std::string prefix = "/files/";
        if (state.request.method == "PUT" && state.request.path.compare(0, prefix.size(), prefix) == 0) {
            if (!decodeFilename(state.request.path.substr(prefix.size()), state.filename)) return respond(channel, 400);
            if (state.request.content_length > MAX_UPLOAD_BYTES) return respond(channel, 413);
        }
        state.header_complete = true;
        state.body_received = state.header.size() - headerSize;
        if (state.body_received > state.request.content_length) return respond(channel, 400);
        if (!state.filename.empty()) state.body.append(state.header.data() + headerSize, state.body_received);
        state.header.clear();
    } else {
        if (size > state.request.content_length - state.body_received) return respond(channel, 400);
        if (!state.filename.empty()) state.body.append(data, size);
        state.body_received += size;
    }
    if (state.body_received > state.request.content_length) return respond(channel, 400);
    if (state.body_received == state.request.content_length) {
        if (!state.filename.empty()) return saveFile(channel, state.filename, state.body);
        const std::string prefix = "/files/";
        if (state.request.method == "GET" && state.request.path.compare(0, prefix.size(), prefix) == 0) {
            return sendFile(channel, state.request.path.substr(prefix.size()));
        }
        return respond(channel, 200);
    }
    return true;
}

// 客户端断开时删除对应的请求状态。
void HttpHandler::onDisconnected(TcpChannel& channel)
{
    requests_.erase(channel.getFd());
}
