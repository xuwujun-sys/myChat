#include "HttpHandler.h"
#include "HttpResponse.h"
#include "FileService.h"
#include "HttpProtocol.h"
#include "../../network/TcpChannel.h"
#include "../../config/config.h"

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
    // 按客户端 fd 找到对应的请求状态。
    RequestState& state = requests_.at(channel.getFd());
    // 请求头尚未接收完整。
    if (!state.header_complete) {
        // 累积收到的数据，查找请求头结束标记。
        state.header.append(data, size);
        std::size_t end = state.header.find("\r\n\r\n");
        if (end == std::string::npos) {
            // 请求头过大返回 431，否则等待后续数据。
            if (state.header.size() > MAX_HTTP_HEADER_BYTES) return respond(channel, 431);
            return true;
        }
        // 请求头长度包含末尾的四个字节。
        std::size_t headerSize = end + 4;
        if (headerSize > MAX_HTTP_HEADER_BYTES) return respond(channel, 431);
        // 解析完整请求头，结果保存在 state.request 中。
        HttpProtocol protocol;
        int status = protocol.parseHeader(state.header.substr(0, headerSize), state.request);

        // 解析失败时返回对应的错误状态码。
        if (status != 0) return respond(channel, status);
        const std::string prefix = "/files/";
        // PUT /files/文件名表示上传文件。
        if (state.request.method == "PUT" && state.request.path.compare(0, prefix.size(), prefix) == 0) {
            // 校验文件名和声明的上传大小。
            if (!FileService::decodeFilename(state.request.path.substr(prefix.size()), state.filename)) return respond(channel, 400);
            if (state.request.content_length > MAX_UPLOAD_BYTES) return respond(channel, 413);
            // 创建上传对象，并为上传打开临时文件。
            state.upload.reset(new UploadFile());
            int uploadStatus = state.upload->open(state.filename);
            // 打开失败时释放上传对象并返回错误。
            if (uploadStatus != 200) { state.upload.reset(); return respond(channel, uploadStatus); }
        }
        // 标记请求头处理完成，计算同次接收中附带的请求体大小。
        state.header_complete = true;
        state.body_received = state.header.size() - headerSize;
        // 收到的数据超过声明长度时清理上传并返回 400。
        if (state.body_received > state.request.content_length) { state.upload.reset(); return respond(channel, 400); }
        // 将请求头后面附带的上传数据写入临时文件。
        if (state.upload && !state.upload->write(state.header.data() + headerSize, state.body_received)) {
            // 写入失败时清理临时文件并返回 500。
            state.upload.reset();
            return respond(channel, 500);
        }
        // 请求头和附带的请求体已处理，清空暂存数据。
        state.header.clear();
    } else {
        // 后续收到的是请求体；超出剩余长度时返回 400。
        if (size > state.request.content_length - state.body_received) { state.upload.reset(); return respond(channel, 400); }
        // 上传时将本次收到的数据写入临时文件。
        if (state.upload && !state.upload->write(data, size)) {
            // 写入失败时清理上传并返回 500。
            state.upload.reset();
            return respond(channel, 500);
        }
        // 记录目前已收到的请求体字节数。
        state.body_received += size;
    }
    // 再次检查请求体长度。
    if (state.body_received > state.request.content_length) return respond(channel, 400);
    // 当请求体达到声明长度时，处理完整请求。
    if (state.body_received == state.request.content_length) {
        if (state.upload) {
            // 完成上传，释放上传对象并返回结果。
            int uploadStatus = state.upload->finish();
            state.upload.reset();
            return respond(channel, uploadStatus);
        }
        const std::string prefix = "/files/";
        // GET /files/文件名表示开始下载。
        if (state.request.method == "GET" && state.request.path.compare(0, prefix.size(), prefix) == 0) {
            return startDownload(channel, state, state.request.path.substr(prefix.size()));
        }
        // 其他完整请求返回固定响应。
        return respond(channel, 200);
    }
    // 请求体尚未接收完整，等待后续数据。
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
    // 按客户端 fd 找到对应的请求状态。
    RequestState& state = requests_.at(channel.getFd());
    // 没有正在下载的文件，无需继续处理。
    if (state.download_file.get() < 0) return true;
    // 文件已经读取完，且之前排队的数据也已发送完。
    if (state.download_remaining == 0) {
        // 关闭文件，标记当前连接发送完后关闭。
        state.download_file.close();
        channel.closeAfterSend();
        return true;
    }
    // 每次最多读取 64 KiB，最后一块可能更小。
    const std::size_t chunkSize = 64 * 1024;
    std::size_t size = state.download_remaining < chunkSize ? state.download_remaining : chunkSize;
    // 读取下一块文件数据，失败时通知调用方关闭连接。
    std::string chunk;
    if (!FileService::readFile(state.download_file, size, chunk)) return false;
    // 更新剩余字节数，并将数据放入待发送缓冲区。
    state.download_remaining -= size;
    return channel.queueSend(chunk.data(), chunk.size());
}

// 客户端断开时删除对应的请求状态。
void HttpHandler::onDisconnected(TcpChannel& channel)
{
    requests_.erase(channel.getFd());
}
