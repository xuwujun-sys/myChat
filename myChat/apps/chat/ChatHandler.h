#pragma once
#include "../../network/Handler.h"
#include "ChatProtocol.h"
#include "../../driver/mysql/UserRepository.h"
#include <string>
#include <unordered_map>

// 单线程群聊处理器；通道由 Kernel 持有，断开时删除对应状态。
class ChatHandler : public Handler {
public:
    explicit ChatHandler(UserRepository& users) : users_(users) {}
    void onConnected(TcpChannel& channel) override;
    bool onData(TcpChannel& channel, const char* data, std::size_t size) override;
    void onDisconnected(TcpChannel& channel) override;

private:
    UserRepository& users_;
    struct ClientState {
        TcpChannel* channel = nullptr;
        std::string nickname;
        ChatProtocol protocol;
    };
    std::unordered_map<int, ClientState> clients_;
};
