#include "ChatHandler.h"
#include "../../network/TcpChannel.h"

namespace {
// 将一条协议响应放入通道的发送缓冲区。
bool reply(TcpChannel& channel, const std::string& message)
{
    return channel.queueSend(message.data(), message.size());
}

// 统一错误响应格式，返回发送数据入队的结果。
bool sendError(TcpChannel& channel, const std::string& code)
{
    return reply(channel, "ERROR " + code + "\n");
}
}

void ChatHandler::onConnected(TcpChannel& channel)
{
    clients_[channel.getFd()].channel = &channel;
}

bool ChatHandler::onData(TcpChannel& channel, const char* data, std::size_t size)
{
    ClientState& client = clients_.at(channel.getFd());
    std::vector<std::string> lines;
    if (!client.protocol.append(data, size, lines)) {
        bool ok = sendError(channel, "MESSAGE_TOO_LONG");
        channel.closeAfterSend();
        return ok;
    }
    for (const std::string& line : lines) {
        std::string command, value;
        if (!ChatProtocol::parseCommand(line, command, value)) {
            if (!sendError(channel, "INVALID_COMMAND")) return false;
            continue;
        }
        if (command == "LOGIN") {
            if (!client.nickname.empty()) {
                if (!sendError(channel, "ALREADY_LOGGED_IN")) return false;
                continue;
            }
            bool taken = false;
            for (const auto& item : clients_) {
                if (item.second.nickname == value) { taken = true; break; }
            }
            if (taken) {
                if (!sendError(channel, "NICKNAME_TAKEN")) return false;
                continue;
            }
            client.nickname = value;
            if (!reply(channel, "OK LOGIN\n")) return false;
        } else {
            if (client.nickname.empty()) {
                if (!sendError(channel, "LOGIN_REQUIRED")) return false;
                continue;
            }
            std::string message = "CHAT " + client.nickname + " " + value + "\n";
            for (auto it = clients_.begin(); it != clients_.end();) {
                ClientState& recipient = it->second;
                if (it->first != channel.getFd() && !recipient.nickname.empty() &&
                    !recipient.channel->shouldCloseAfterSend() &&
                    !reply(*recipient.channel, message)) {
                    // 接收方积压过多时退出群聊，不影响发送者和其他用户。
                    recipient.channel->closeAfterSend();
                    it = clients_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
    return true;
}

void ChatHandler::onDisconnected(TcpChannel& channel)
{
    clients_.erase(channel.getFd());
}
