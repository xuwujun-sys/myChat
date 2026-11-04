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
            state.upload.reset(new UploadFile());
            int uploadStatus = state.upload->open(state.filename);
            if (uploadStatus != 200) { state.upload.reset(); return respond(channel, uploadStatus); }
        }
        state.header_complete = true;
        state.body_received = state.header.size() - headerSize;
        if (state.body_received > state.request.content_length) { state.upload.reset(); return respond(channel, 400); }
        if (state.upload && !state.upload->write(state.header.data() + headerSize, state.body_received)) {
            state.upload.reset();
            return respond(channel, 500);
        }
        state.header.clear();
    } else {
        if (size > state.request.content_length - state.body_received) { state.upload.reset(); return respond(channel, 400); }
        if (state.upload && !state.upload->write(data, size)) {
            state.upload.reset();
            return respond(channel, 500);
        }
        state.body_received += size;
    }
    if (state.body_received > state.request.content_length) return respond(channel, 400);
    if (state.body_received == state.request.content_length) {
        if (state.upload) {
            int uploadStatus = state.upload->finish();
            state.upload.reset();
            return respond(channel, uploadStatus);
        }
        const std::string prefix = "/files/";
        if (state.request.method == "GET" && state.request.path.compare(0, prefix.size(), prefix) == 0) {
            return startDownload(channel, state, state.request.path.substr(prefix.size()));
        }
        return respond(channel, 200);
    }
    return true;
}

// 先发送响应头，文件内容在发送缓冲清空后继续发送。
bool HttpHandler::startDownload(TcpChannel& channel, RequestState& state, const std::string& path)
{
    int status = FileService::openFile(path, state.download_file, state.download_remaining);
    if (status != 200) return respond(channel, status);
    HttpResponse response(200, "application/octet-stream", state.download_remaining);
    std::string header = response.buildHeader();
    return channel.queueSend(header.data(), header.size());
}

// 每次发送一块，最后一块发送完毕后关闭连接。
bool HttpHandler::onSendComplete(TcpChannel& channel)
{
    RequestState& state = requests_.at(channel.getFd());
    if (state.download_file.get() < 0) return true;
    if (state.download_remaining == 0) {
        state.download_file.close();
        channel.closeAfterSend();
        return true;
    }
    const std::size_t chunkSize = 64 * 1024;
    std::size_t size = state.download_remaining < chunkSize ? state.download_remaining : chunkSize;
    std::string chunk;
    if (!FileService::readFile(state.download_file, size, chunk)) return false;
    state.download_remaining -= size;
    return channel.queueSend(chunk.data(), chunk.size());
}

// 客户端断开时删除对应的请求状态。
void HttpHandler::onDisconnected(TcpChannel& channel)
{
    requests_.erase(channel.getFd());
}
