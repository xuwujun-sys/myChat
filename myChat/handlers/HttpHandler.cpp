#include "HttpHandler.h"
#include "../core/TcpChannel.h"

void HttpHandler::onConnected(TcpChannel&) {}

bool HttpHandler::onData(TcpChannel& channel, const char*, std::size_t)
{
    static const char response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: 13\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello, HTTP!\n";
    if (!channel.queueSend(response, sizeof(response) - 1)) return false;
    channel.closeAfterSend();
    return true;
}

void HttpHandler::onDisconnected(TcpChannel&) {}
