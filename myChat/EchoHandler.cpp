#include "EchoHandler.h"
#include "TcpChannel.h"

void EchoHandler::onConnected(TcpChannel&) {}

bool EchoHandler::onData(TcpChannel& channel, const char* data, std::size_t size)
{
    return channel.queueSend(data, size);
}

void EchoHandler::onDisconnected(TcpChannel&) {}
