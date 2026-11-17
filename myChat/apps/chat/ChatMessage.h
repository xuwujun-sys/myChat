#pragma once
#include <cstddef>
#include <string>

enum class ChatCommand { Login, Register, Text, Private, CreateGroup };

// 客户端消息的统一接口。
class ChatMessage {
public:
    virtual ~ChatMessage() = default;
    virtual ChatCommand command() const = 0;
    virtual const std::string& value() const = 0;
};

class AccountMessage : public ChatMessage {
public:
    const std::string& value() const override { return value_; }
    const std::string& username() const { return username_; }
    const std::string& password() const { return password_; }

protected:
    AccountMessage(const std::string& value, std::size_t separator)
        : value_(value), username_(value.substr(0, separator)), password_(value.substr(separator + 1)) {}

private:
    std::string value_;
    std::string username_;
    std::string password_;
};

class LoginMessage : public AccountMessage {
public:
    LoginMessage(const std::string& value, std::size_t separator) : AccountMessage(value, separator) {}
    ChatCommand command() const override { return ChatCommand::Login; }
};

class RegisterMessage : public AccountMessage {
public:
    RegisterMessage(const std::string& value, std::size_t separator) : AccountMessage(value, separator) {}
    ChatCommand command() const override { return ChatCommand::Register; }
};

class TextMessage : public ChatMessage {
public:
    explicit TextMessage(const std::string& value) : value_(value) {}
    ChatCommand command() const override { return ChatCommand::Text; }
    const std::string& value() const override { return value_; }

private:
    std::string value_;
};

class CreateGroupMessage : public ChatMessage {
public:
    explicit CreateGroupMessage(const std::string& name) : name_(name) {}
    ChatCommand command() const override { return ChatCommand::CreateGroup; }
    const std::string& value() const override { return name_; }

private:
    std::string name_;
};

class PrivateMessage : public ChatMessage {
public:
    PrivateMessage(const std::string& value, std::size_t separator)
        : value_(value), receiver_(value.substr(0, separator)), content_(value.substr(separator + 1)) {}
    ChatCommand command() const override { return ChatCommand::Private; }
    const std::string& value() const override { return value_; }
    const std::string& receiver() const { return receiver_; }
    const std::string& content() const { return content_; }

private:
    std::string value_;
    std::string receiver_;
    std::string content_;
};
