#pragma once
#include <cstddef>
#include <string>
#include <vector>

// 每个连接使用一个实例，保存尚未收到换行符的数据。
class ChatProtocol {
public:
    // 提取客户端命令行（不含换行符）；行过长时返回 false，调用方应关闭连接。
    bool append(const char* data, std::size_t size, std::vector<std::string>& lines);

    // 校验一行客户端命令，并返回命令名和参数。
    static bool parseCommand(const std::string& line, std::string& command, std::string& value);

private:
    std::string pending_;
};
