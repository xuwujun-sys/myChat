#include "HttpHandler.h"
#include "HttpResponse.h"
#include "FileService.h"
#include "../protocol/HttpProtocol.h"
#include "../core/TcpChannel.h"
#include "../config/config.h"

namespace {

// 按状态码生成响应，发送完成后关闭连接。
bool respond(TcpChannel& channel, int status)
{
    HttpResponse response(status);
    std::string message = response.buildMessage();
    if (!channel.queueSend(message.data(), message.size())) return false;
    channel.closeAfterSend();
    return true;
}

// 读取小文件并生成下载响应。
bool sendFile(TcpChannel& channel, const std::string& path)
{
    UniqueFd file;
    std::size_t size = 0;
    int status = FileService::openFile(path, file, size);
    if (status != 200) return respond(channel, status);
    HttpResponse response(200, "application/octet-stream", size);
    std::string message = response.buildHeader();
    if (size > MAX_PENDING_BYTES - message.size()) return respond(channel, 413);
    std::string body;
    if (!FileService::readFile(file, size, body)) return respond(channel, 500);
    file.close();
    message += body;
    if (!channel.queueSend(message.data(), message.size())) return false;
    channel.closeAfterSend();
    return true;
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
        HttpProtocol protocol;
        int status = protocol.parseHeader(state.header.substr(0, headerSize), state.request);

        if (status != 0) return respond(channel, status);
        const std::string prefix = "/files/";
        if (state.request.method == "PUT" && state.request.path.compare(0, prefix.size(), prefix) == 0) {
            if (!FileService::decodeFilename(state.request.path.substr(prefix.size()), state.filename)) return respond(channel, 400);
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
        if (!state.filename.empty()) return respond(channel, FileService::saveFile(state.filename, state.body));
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
