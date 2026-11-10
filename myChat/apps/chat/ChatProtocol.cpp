#include "ChatProtocol.h"
#include "../../config/config.h"

bool ChatProtocol::append(const char* data, std::size_t size, std::vector<std::string>& lines)
{
    const std::size_t loginLimit = 6 + MAX_CHAT_NICKNAME_BYTES; // LOGIN、空格和昵称。
    const std::size_t messageLimit = 4 + MAX_CHAT_MESSAGE_BYTES; // MSG、空格和内容。
    const std::size_t maxLineBytes = loginLimit > messageLimit ? loginLimit : messageLimit;
    for (std::size_t i = 0; i < size; ++i) {
        if (data[i] == '\n') {
            lines.push_back(pending_);
            pending_.clear();
        } else {
            if (pending_.size() >= maxLineBytes) return false;
            pending_.push_back(data[i]);
        }
    }
    return true;
}

bool ChatProtocol::parseCommand(const std::string& line, std::string& command, std::string& value)
{
    std::size_t separator = line.find(' ');
    if (separator == std::string::npos) return false;
    std::string name = line.substr(0, separator);
    std::string argument = line.substr(separator + 1);
    if (name == "LOGIN") {
        if (argument.empty() || argument.size() > MAX_CHAT_NICKNAME_BYTES) return false;
        for (unsigned char c : argument) if (c <= 32 || c == 127) return false;
    } else if (name == "MSG") {
        if (argument.empty() || argument.size() > MAX_CHAT_MESSAGE_BYTES) return false;
        for (unsigned char c : argument) if (c < 32 || c == 127) return false;
    } else {
        return false;
    }
    command = name;
    value = argument;
    return true;
}
