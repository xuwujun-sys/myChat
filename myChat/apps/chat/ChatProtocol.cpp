#include "ChatProtocol.h"
#include "../../config/config.h"

namespace {
std::unique_ptr<ChatMessage> parseAccount(const std::string& name, const std::string& argument)
{
    std::size_t space = argument.find(' ');
    if (space == std::string::npos || space == 0 || space > MAX_CHAT_NICKNAME_BYTES ||
        argument.size() - space - 1 == 0 ||
        argument.size() - space - 1 > MAX_CHAT_PASSWORD_BYTES) return nullptr;
    for (unsigned char c : argument) if ((c <= 32 && c != ' ') || c == 127) return nullptr;
    if (argument.find(' ', space + 1) != std::string::npos) return nullptr;
    if (name == "LOGIN") return std::unique_ptr<ChatMessage>(new LoginMessage(argument));
    return std::unique_ptr<ChatMessage>(new RegisterMessage(argument));
}

std::unique_ptr<ChatMessage> parsePrivate(const std::string& argument)
{
    std::size_t space = argument.find(' ');
    if (space == std::string::npos || space == 0 || space > MAX_CHAT_NICKNAME_BYTES ||
        argument.size() - space - 1 == 0 || argument.size() - space - 1 > MAX_CHAT_MESSAGE_BYTES) return nullptr;
    for (std::size_t i = 0; i < space; ++i) {
        unsigned char c = argument[i];
        if (c <= 32 || c == 127) return nullptr;
    }
    for (std::size_t i = space + 1; i < argument.size(); ++i) {
        unsigned char c = argument[i];
        if (c < 32 || c == 127) return nullptr;
    }
    return std::unique_ptr<ChatMessage>(new PrivateMessage(argument));
}

std::unique_ptr<ChatMessage> parseText(const std::string& argument)
{
    if (argument.empty() || argument.size() > MAX_CHAT_MESSAGE_BYTES) return nullptr;
    for (unsigned char c : argument) if (c < 32 || c == 127) return nullptr;
    return std::unique_ptr<ChatMessage>(new TextMessage(argument));
}
}

bool ChatProtocol::append(const char* data, std::size_t size, std::vector<std::string>& lines)
{
    const std::size_t loginLimit = 10 + MAX_CHAT_NICKNAME_BYTES + MAX_CHAT_PASSWORD_BYTES; // REGISTER、空格和账号密码。
    const std::size_t messageLimit = 9 + MAX_CHAT_NICKNAME_BYTES + MAX_CHAT_MESSAGE_BYTES; // PRIVATE、空格、接收人和内容。
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

std::unique_ptr<ChatMessage> ChatProtocol::parseCommand(const std::string& line)
{
    std::size_t separator = line.find(' ');
    if (separator == std::string::npos) return nullptr;
    std::string name = line.substr(0, separator);
    std::string argument = line.substr(separator + 1);
    if (name == "LOGIN" || name == "REGISTER") return parseAccount(name, argument);
    if (name == "PRIVATE") return parsePrivate(argument);
    if (name == "MSG") return parseText(argument);
    return nullptr;
}
