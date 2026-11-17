#pragma once
#include <string>

enum class ChatCommand { Login, Register, Text, Private };

// 客户端消息的统一接口。
class ChatMessage {
public:
    virtual ~ChatMessage() = default;
    virtual ChatCommand command() const = 0;
    virtual const std::string& value() const = 0;
};

class LoginMessage : public ChatMessage {
public:
    explicit LoginMessage(const std::string& value) : value_(value) {}
    ChatCommand command() const override { return ChatCommand::Login; }
    const std::string& value() const override { return value_; }

private:
    std::string value_;
};

class RegisterMessage : public ChatMessage {
public:
    explicit RegisterMessage(const std::string& value) : value_(value) {}
    ChatCommand command() const override { return ChatCommand::Register; }
    const std::string& value() const override { return value_; }

private:
    std::string value_;
};

class TextMessage : public ChatMessage {
public:
    explicit TextMessage(const std::string& value) : value_(value) {}
    ChatCommand command() const override { return ChatCommand::Text; }
    const std::string& value() const override { return value_; }

private:
    std::string value_;
};

class PrivateMessage : public ChatMessage {
public:
    explicit PrivateMessage(const std::string& value) : value_(value) {}
    ChatCommand command() const override { return ChatCommand::Private; }
    const std::string& value() const override { return value_; }

private:
    std::string value_;
};
