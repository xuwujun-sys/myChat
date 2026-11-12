#pragma once
#include <string>

// 客户端消息的统一接口。
class ChatMessage {
public:
    virtual ~ChatMessage() = default;
    virtual const char* command() const = 0;
    virtual const std::string& value() const = 0;
};

class LoginMessage : public ChatMessage {
public:
    explicit LoginMessage(const std::string& value) : value_(value) {}
    const char* command() const override { return "LOGIN"; }
    const std::string& value() const override { return value_; }

private:
    std::string value_;
};

class RegisterMessage : public ChatMessage {
public:
    explicit RegisterMessage(const std::string& value) : value_(value) {}
    const char* command() const override { return "REGISTER"; }
    const std::string& value() const override { return value_; }

private:
    std::string value_;
};

class TextMessage : public ChatMessage {
public:
    explicit TextMessage(const std::string& value) : value_(value) {}
    const char* command() const override { return "MSG"; }
    const std::string& value() const override { return value_; }

private:
    std::string value_;
};
