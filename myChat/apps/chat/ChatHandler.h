#pragma once
#include "../../network/Handler.h"
#include "ChatProtocol.h"
#include "../../driver/mysql/UserRepository.h"
#include "../../driver/mysql/GroupRepository.h"
#include <string>
#include <unordered_map>

// 单线程群聊处理器；通道由 Kernel 持有，断开时删除对应状态。
class ChatHandler : public Handler {
public:
    ChatHandler(UserRepository& users, GroupRepository& groups) : users_(users), groups_(groups) {}
    void onConnected(TcpChannel& channel) override;
    bool onData(TcpChannel& channel, const char* data, std::size_t size) override;
    void onDisconnected(TcpChannel& channel) override;

private:
    UserRepository& users_;
    GroupRepository& groups_;
    struct ClientState {
        TcpChannel* channel = nullptr;
        std::string nickname;
        std::uint64_t user_id = 0;
        ChatProtocol protocol;
    };
    std::unordered_map<int, ClientState> clients_;

    // requireOpen 为 true 时跳过发送完后即将关闭的连接。
    ClientState* findClientByNickname(const std::string& nickname, bool requireOpen = false);
    void markOffline(ClientState& client); // 标记关闭并清空昵称；不广播名单。
    bool handleLogin(TcpChannel& channel, ClientState& client,
                     const std::string& username, const std::string& password);
    bool handleRegister(TcpChannel& channel, const std::string& username, const std::string& password);
    bool broadcastMessage(TcpChannel& channel, const ClientState& client, const std::string& value);
    bool handlePrivateMessage(TcpChannel& channel, const ClientState& client, const PrivateMessage& message);
    bool handleCreateGroup(TcpChannel& channel, const ClientState& client, const std::string& name);
    bool handleMyGroups(TcpChannel& channel, const ClientState& client);
    bool handleJoinGroup(TcpChannel& channel, const ClientState& client, std::uint64_t groupId);
    void broadcastUsers();
};
