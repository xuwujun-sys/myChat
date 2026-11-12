#include "../apps/chat/ChatProtocol.h"
#include "../config/config.h"
#include <cassert>
#include <string>
#include <vector>

static bool parseCommand(const std::string& line, std::string& command, std::string& value)
{
    std::unique_ptr<ChatMessage> message = ChatProtocol::parseCommand(line);
    if (!message) return false;
    command = message->command();
    value = message->value();
    return true;
}

int main()
{
    ChatProtocol first, second;
    std::vector<std::string> lines;
    std::string command, value;

    assert(dynamic_cast<LoginMessage*>(ChatProtocol::parseCommand("LOGIN user pass").get()));
    assert(dynamic_cast<RegisterMessage*>(ChatProtocol::parseCommand("REGISTER user pass").get()));
    assert(dynamic_cast<TextMessage*>(ChatProtocol::parseCommand("MSG hello").get()));

    assert(first.append("LOG", 3, lines) && lines.empty());
    assert(second.append("MSG other\n", 10, lines) && lines.size() == 1);
    assert(lines[0] == "MSG other");
    lines.clear();
    const std::string rest = "IN user pass\nMSG hello world\nMSG next\n";
    assert(first.append(rest.data(), rest.size(), lines) && lines.size() == 3);
    assert(parseCommand(lines[0], command, value) && command == "LOGIN" && value == "user pass");
    assert(parseCommand(lines[1], command, value) && command == "MSG" && value == "hello world");
    assert(parseCommand(lines[2], command, value) && value == "next");

    assert(!parseCommand("LOGIN ", command, value));
    assert(!parseCommand("LOGIN user", command, value));
    assert(!parseCommand("LOGIN user pass extra", command, value));
    assert(!parseCommand("LOGIN " + std::string(MAX_CHAT_NICKNAME_BYTES + 1, 'n') + " pass", command, value));
    assert(parseCommand("LOGIN " + std::string(MAX_CHAT_NICKNAME_BYTES, 'n') + " pass", command, value));
    assert(parseCommand("LOGIN \xE5\xB0\x8F\xE6\x98\x8E pass", command, value));
    assert(parseCommand("REGISTER user pass", command, value) && command == "REGISTER");
    assert(!parseCommand("REGISTER user ", command, value));
    assert(!parseCommand("LOGIN user " + std::string(MAX_CHAT_PASSWORD_BYTES + 1, 'x'), command, value));
    assert(!parseCommand("MSG ", command, value));
    assert(!parseCommand("MSG hello\r", command, value));
    assert(!parseCommand(std::string("MSG a\0b", 7), command, value));
    assert(!parseCommand("OTHER text", command, value));
    assert(!parseCommand("MSG " + std::string(MAX_CHAT_MESSAGE_BYTES + 1, 'x'), command, value));
    assert(parseCommand("MSG " + std::string(MAX_CHAT_MESSAGE_BYTES, 'x'), command, value));

    ChatProtocol large;
    lines.clear();
    std::string valid = "MSG " + std::string(MAX_CHAT_MESSAGE_BYTES, 'x') + "\n";
    assert(large.append(valid.data(), valid.size() - 1, lines) && lines.empty());
    assert(large.append(valid.data() + valid.size() - 1, 1, lines) && lines.size() == 1);
    ChatProtocol oversized;
    std::string invalid = "MSG " + std::string(MAX_CHAT_MESSAGE_BYTES + 1, 'x');
    assert(!oversized.append(invalid.data(), invalid.size(), lines));
    return 0;
}
