#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include "ChatMessage.h"

// 每个连接使用一个实例，保存尚未收到换行符的数据。
class ChatProtocol {
public:
    // 提取客户端命令行（不含换行符）；行过长时返回 false，调用方应关闭连接。
    bool append(const char* data, std::size_t size, std::vector<std::string>& lines);

    // 校验一行客户端命令，成功时返回对应的消息对象。
    static std::unique_ptr<ChatMessage> parseCommand(const std::string& line);

private:
    std::string pending_;
};
