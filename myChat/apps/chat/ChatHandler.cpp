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
        if (!client.nickname.empty()) {
            client.nickname.clear();
            broadcastUsers();
        }
        return ok;
    }
    for (const std::string& line : lines) {
        std::unique_ptr<ChatMessage> chatMessage = ChatProtocol::parseCommand(line);
        if (!chatMessage) {
            if (!sendError(channel, "INVALID_COMMAND")) return false;
            continue;
        }
        std::string command = chatMessage->command();
        const std::string& value = chatMessage->value();
        if (command == "LOGIN" || command == "REGISTER") {
            if (!client.nickname.empty()) {
                if (!sendError(channel, "ALREADY_LOGGED_IN")) return false;
                continue;
            }
            std::size_t space = value.find(' ');
            std::string username = value.substr(0, space);
            std::string password = value.substr(space + 1);
            if (command == "REGISTER") {
                if (!handleRegister(channel, username, password)) return false;
            } else {
                if (!handleLogin(channel, client, username, password)) return false;
            }
        } else {
            if (!broadcastMessage(channel, client, value)) return false;
        }
        if (channel.shouldCloseAfterSend()) return true;
    }
    return true;
}

bool ChatHandler::handleRegister(TcpChannel& channel, const std::string& username, const std::string& password)
{
    UserResult result = users_.createUser(username, password);
    if (result == UserResult::AlreadyExists) return sendError(channel, "NICKNAME_TAKEN");
    if (result != UserResult::Success) return sendError(channel, "DATABASE_ERROR");
    return reply(channel, "OK REGISTER\n");
}

bool ChatHandler::handleLogin(TcpChannel& channel, ClientState& client,
                              const std::string& username, const std::string& password)
{
    User user;
    UserResult result = users_.findUser(username, user);
    if (result == UserResult::DatabaseError) return sendError(channel, "DATABASE_ERROR");
    if (result != UserResult::Success || user.password != password) return sendError(channel, "INVALID_CREDENTIALS");

    bool taken = false;
    for (const auto& item : clients_) {
        if (item.second.nickname == username) { taken = true; break; }
    }
    if (taken) return sendError(channel, "NICKNAME_TAKEN");
    client.nickname = username;
    if (!reply(channel, "OK LOGIN\n")) return false;
    broadcastUsers();
    return true;
}

bool ChatHandler::broadcastMessage(TcpChannel& channel, const ClientState& client, const std::string& value)
{
    if (client.nickname.empty()) return sendError(channel, "LOGIN_REQUIRED");
    std::string message = "CHAT " + client.nickname + " " + value + "\n";
    bool usersChanged = false;
    for (auto it = clients_.begin(); it != clients_.end();) {
        ClientState& recipient = it->second;
        if (it->first != channel.getFd() && !recipient.nickname.empty() &&
            !recipient.channel->shouldCloseAfterSend() &&
            !reply(*recipient.channel, message)) {
            // 接收方积压过多时退出群聊，不影响发送者和其他用户。
            recipient.channel->closeAfterSend();
            recipient.nickname.clear();
            usersChanged = true;
        }
        ++it;
    }
    if (usersChanged) broadcastUsers();
    return true;
}

void ChatHandler::broadcastUsers()
{
    bool usersChanged;
    do {
        usersChanged = false;
        std::string message = "USERS";
        for (const auto& item : clients_) {
            if (!item.second.nickname.empty()) message += " " + item.second.nickname;
        }
        message += "\n";
        for (auto& item : clients_) {
            ClientState& client = item.second;
            if (!client.nickname.empty() && !reply(*client.channel, message)) {
                client.channel->closeAfterSend();
                client.nickname.clear();
                usersChanged = true;
            }
        }
    } while (usersChanged);
}

void ChatHandler::onDisconnected(TcpChannel& channel)
{
    auto it = clients_.find(channel.getFd());
    if (it == clients_.end()) return;
    bool wasOnline = !it->second.nickname.empty();
    clients_.erase(it);
    if (wasOnline) broadcastUsers();
}
