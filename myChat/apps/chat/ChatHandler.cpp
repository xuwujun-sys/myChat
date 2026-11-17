#include "ChatHandler.h"
#include "../../network/TcpChannel.h"
#include <algorithm>
#include <iostream>

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

ChatHandler::ClientState* ChatHandler::findClientByNickname(const std::string& nickname, bool requireOpen)
{
    for (auto& item : clients_) {
        ClientState& client = item.second;
        if (client.nickname == nickname && (!requireOpen || !client.channel->shouldCloseAfterSend())) return &client;
    }
    return nullptr;
}

void ChatHandler::markOffline(ClientState& client)
{
    client.channel->closeAfterSend();
    client.nickname.clear();
    client.user_id = 0;
}

bool ChatHandler::onData(TcpChannel& channel, const char* data, std::size_t size)
{
    ClientState& client = clients_.at(channel.getFd());
    std::vector<std::string> lines;
    if (!client.protocol.append(data, size, lines)) {
        bool ok = sendError(channel, "MESSAGE_TOO_LONG");
        bool wasOnline = !client.nickname.empty();
        markOffline(client);
        if (wasOnline) broadcastUsers();
        return ok;
    }
    for (const std::string& line : lines) {
        std::unique_ptr<ChatMessage> chatMessage = ChatProtocol::parseCommand(line);
        if (!chatMessage) {
            if (!sendError(channel, "INVALID_COMMAND")) return false;
            continue;
        }
        ChatCommand command = chatMessage->command();
        const std::string& value = chatMessage->value();
        switch (command) {
        case ChatCommand::Login:
        case ChatCommand::Register: {
            if (!client.nickname.empty()) {
                if (!sendError(channel, "ALREADY_LOGGED_IN")) return false;
                continue;
            }
            const AccountMessage* account = dynamic_cast<const AccountMessage*>(chatMessage.get());
            if (!account) return false;
            if (command == ChatCommand::Register) {
                if (!handleRegister(channel, account->username(), account->password())) return false;
            } else {
                if (!handleLogin(channel, client, account->username(), account->password())) return false;
            }
            break;
        }
        case ChatCommand::Private: {
            const PrivateMessage* message = dynamic_cast<const PrivateMessage*>(chatMessage.get());
            if (!message || !handlePrivateMessage(channel, client, *message)) return false;
            break;
        }
        case ChatCommand::Text:
            if (!broadcastMessage(channel, client, value)) return false;
            break;
        case ChatCommand::CreateGroup:
            if (!handleCreateGroup(channel, client, value)) return false;
            break;
        case ChatCommand::MyGroups:
            if (!handleMyGroups(channel, client)) return false;
            break;
        case ChatCommand::JoinGroup: {
            const JoinGroupMessage* message = dynamic_cast<const JoinGroupMessage*>(chatMessage.get());
            if (!message || !handleJoinGroup(channel, client, message->groupId())) return false;
            break;
        }
        case ChatCommand::GroupText: {
            const GroupTextMessage* message = dynamic_cast<const GroupTextMessage*>(chatMessage.get());
            if (!message || !handleGroupText(channel, client, *message)) return false;
            break;
        }
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

    if (findClientByNickname(username)) return sendError(channel, "NICKNAME_TAKEN");
    client.nickname = username;
    client.user_id = user.id;
    if (!reply(channel, "OK LOGIN\n")) return false;
    broadcastUsers();
    return true;
}

bool ChatHandler::handleCreateGroup(TcpChannel& channel, const ClientState& client, const std::string& name)
{
    if (client.nickname.empty()) return sendError(channel, "LOGIN_REQUIRED");
    std::uint64_t groupId = 0;
    GroupResult result = groups_.createGroup(name, client.user_id, groupId);
    if (result == GroupResult::CommitUnknown) return sendError(channel, "CREATE_GROUP_UNCERTAIN " + std::to_string(groupId));
    if (result == GroupResult::SuccessConnectionLost) {
        std::cerr << "Group created, database connection lost: " << groups_.lastError() << std::endl;
    } else if (result != GroupResult::Success) {
        return sendError(channel, "DATABASE_ERROR");
    }
    return reply(channel, "OK CREATE_GROUP " + std::to_string(groupId) + " " + name + "\n");
}

bool ChatHandler::handleMyGroups(TcpChannel& channel, const ClientState& client)
{
    if (client.nickname.empty()) return sendError(channel, "LOGIN_REQUIRED");
    std::vector<Group> groups;
    if (groups_.listGroups(client.user_id, groups) != GroupResult::Success) return sendError(channel, "DATABASE_ERROR");

    // 避免数据库中已有的非法群名破坏按行分隔的协议。
    for (const Group& group : groups) {
        if (group.name.empty()) return sendError(channel, "DATABASE_ERROR");
        for (unsigned char c : group.name) if (c < 32 || c == 127) return sendError(channel, "DATABASE_ERROR");
    }
    if (!reply(channel, "MY_GROUPS_BEGIN\n")) return false;
    for (const Group& group : groups) {
        if (!reply(channel, "MY_GROUP " + std::to_string(group.id) + " " + group.name + "\n")) return false;
    }
    return reply(channel, "MY_GROUPS_END\n");
}

bool ChatHandler::handleJoinGroup(TcpChannel& channel, const ClientState& client, std::uint64_t groupId)
{
    if (client.nickname.empty()) return sendError(channel, "LOGIN_REQUIRED");
    GroupResult result = groups_.addMember(groupId, client.user_id);
    std::string id = std::to_string(groupId);
    if (result == GroupResult::AlreadyMember) return sendError(channel, "ALREADY_GROUP_MEMBER " + id);
    if (result == GroupResult::NotFound) return sendError(channel, "GROUP_OR_USER_NOT_FOUND " + id);
    if (result != GroupResult::Success) return sendError(channel, "DATABASE_ERROR");
    return reply(channel, "OK JOIN_GROUP " + id + "\n");
}

bool ChatHandler::handleGroupText(TcpChannel& channel, const ClientState& client, const GroupTextMessage& message)
{
    if (client.nickname.empty()) return sendError(channel, "LOGIN_REQUIRED");
    std::vector<std::uint64_t> members;
    if (groups_.listMemberIds(message.groupId(), members) != GroupResult::Success) return sendError(channel, "DATABASE_ERROR");
    if (std::find(members.begin(), members.end(), client.user_id) == members.end()) return sendError(channel, "NOT_GROUP_MEMBER");

    std::string text = "GROUP_CHAT " + std::to_string(message.groupId()) + " " + client.nickname + " " + message.content() + "\n";
    bool usersChanged = false;
    for (auto& item : clients_) {
        ClientState& recipient = item.second;
        if (item.first != channel.getFd() && !recipient.nickname.empty() &&
            !recipient.channel->shouldCloseAfterSend() &&
            std::find(members.begin(), members.end(), recipient.user_id) != members.end() &&
            !reply(*recipient.channel, text)) {
            markOffline(recipient);
            usersChanged = true;
        }
    }
    if (usersChanged) broadcastUsers();
    return true;
}

bool ChatHandler::handlePrivateMessage(TcpChannel& channel, const ClientState& client, const PrivateMessage& privateMessage)
{
    if (client.nickname.empty()) return sendError(channel, "LOGIN_REQUIRED");
    const std::string& recipientName = privateMessage.receiver();
    if (recipientName == client.nickname) return sendError(channel, "INVALID_RECIPIENT");
    ClientState* recipient = findClientByNickname(recipientName, true);
    if (!recipient) return sendError(channel, "USER_OFFLINE " + recipientName);
    std::string message = "PRIVATE " + client.nickname + " " + privateMessage.content() + "\n";
    if (reply(*recipient->channel, message)) return true;
    markOffline(*recipient);
    bool ok = sendError(channel, "SEND_FAILED " + recipientName);
    broadcastUsers();
    return ok;
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
            markOffline(recipient);
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
                markOffline(client);
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
