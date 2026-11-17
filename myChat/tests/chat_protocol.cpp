#include "../apps/chat/ChatProtocol.h"
#include "../config/config.h"
#include <cassert>
#include <string>
#include <vector>

static bool parseCommand(const std::string& line, ChatCommand& command, std::string& value)
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
    ChatCommand command = ChatCommand::Login;
    std::string value;

    assert(dynamic_cast<LoginMessage*>(ChatProtocol::parseCommand("LOGIN user pass").get()));
    assert(dynamic_cast<RegisterMessage*>(ChatProtocol::parseCommand("REGISTER user pass").get()));
    assert(dynamic_cast<TextMessage*>(ChatProtocol::parseCommand("MSG hello").get()));
    assert(dynamic_cast<PrivateMessage*>(ChatProtocol::parseCommand("PRIVATE user hello world").get()));
    assert(dynamic_cast<CreateGroupMessage*>(ChatProtocol::parseCommand("CREATE_GROUP Project Alpha").get()));
    assert(dynamic_cast<MyGroupsMessage*>(ChatProtocol::parseCommand("MY_GROUPS").get()));

    std::unique_ptr<ChatMessage> login = ChatProtocol::parseCommand("LOGIN alice pass");
    const AccountMessage* account = dynamic_cast<const AccountMessage*>(login.get());
    assert(account && account->username() == "alice" && account->password() == "pass");
    std::unique_ptr<ChatMessage> registration = ChatProtocol::parseCommand("REGISTER bob secret");
    account = dynamic_cast<const AccountMessage*>(registration.get());
    assert(account && account->username() == "bob" && account->password() == "secret");
    std::unique_ptr<ChatMessage> privateMessage = ChatProtocol::parseCommand("PRIVATE bob hello world");
    const PrivateMessage* privateData = dynamic_cast<const PrivateMessage*>(privateMessage.get());
    assert(privateData && privateData->receiver() == "bob" && privateData->content() == "hello world");

    assert(first.append("LOG", 3, lines) && lines.empty());
    assert(second.append("MSG other\n", 10, lines) && lines.size() == 1);
    assert(lines[0] == "MSG other");
    lines.clear();
    const std::string rest = "IN user pass\nMSG hello world\nMSG next\n";
    assert(first.append(rest.data(), rest.size(), lines) && lines.size() == 3);
    assert(parseCommand(lines[0], command, value) && command == ChatCommand::Login && value == "user pass");
    assert(parseCommand(lines[1], command, value) && command == ChatCommand::Text && value == "hello world");
    assert(parseCommand(lines[2], command, value) && value == "next");

    assert(!parseCommand("LOGIN ", command, value));
    assert(!parseCommand("LOGIN user", command, value));
    assert(!parseCommand("LOGIN user pass extra", command, value));
    assert(!parseCommand("LOGIN " + std::string(MAX_CHAT_NICKNAME_BYTES + 1, 'n') + " pass", command, value));
    assert(parseCommand("LOGIN " + std::string(MAX_CHAT_NICKNAME_BYTES, 'n') + " pass", command, value));
    assert(parseCommand("LOGIN \xE5\xB0\x8F\xE6\x98\x8E pass", command, value));
    assert(parseCommand("REGISTER user pass", command, value) && command == ChatCommand::Register);
    assert(!parseCommand("REGISTER user ", command, value));
    assert(!parseCommand("LOGIN user " + std::string(MAX_CHAT_PASSWORD_BYTES + 1, 'x'), command, value));
    assert(!parseCommand("MSG ", command, value));
    assert(!parseCommand("MSG hello\r", command, value));
    assert(!parseCommand(std::string("MSG a\0b", 7), command, value));
    assert(!parseCommand("OTHER text", command, value));
    assert(!parseCommand("MSG " + std::string(MAX_CHAT_MESSAGE_BYTES + 1, 'x'), command, value));
    assert(parseCommand("MSG " + std::string(MAX_CHAT_MESSAGE_BYTES, 'x'), command, value));
    assert(parseCommand("PRIVATE user hello world", command, value) && command == ChatCommand::Private && value == "user hello world");
    assert(!parseCommand("PRIVATE user", command, value));
    assert(!parseCommand("PRIVATE  hello", command, value));
    assert(!parseCommand("PRIVATE user ", command, value));
    assert(!parseCommand("PRIVATE user\tbad hello", command, value));
    assert(!parseCommand("PRIVATE user hi\r", command, value));
    assert(!parseCommand("PRIVATE " + std::string(MAX_CHAT_NICKNAME_BYTES + 1, 'n') + " hi", command, value));
    assert(!parseCommand("PRIVATE user " + std::string(MAX_CHAT_MESSAGE_BYTES + 1, 'x'), command, value));
    assert(parseCommand("PRIVATE " + std::string(MAX_CHAT_NICKNAME_BYTES, 'n') + " " + std::string(MAX_CHAT_MESSAGE_BYTES, 'x'), command, value));
    assert(parseCommand("CREATE_GROUP Project Alpha", command, value) && command == ChatCommand::CreateGroup && value == "Project Alpha");
    assert(parseCommand("CREATE_GROUP \xE8\x81\x8A\xE5\xA4\xA9", command, value));
    assert(!parseCommand("CREATE_GROUP ", command, value));
    assert(!parseCommand("CREATE_GROUP  name", command, value));
    assert(!parseCommand("CREATE_GROUP name ", command, value));
    assert(!parseCommand("CREATE_GROUP name\tbad", command, value));
    assert(!parseCommand("CREATE_GROUP " + std::string(MAX_CHAT_GROUP_NAME_BYTES + 1, 'x'), command, value));
    assert(parseCommand("CREATE_GROUP " + std::string(MAX_CHAT_GROUP_NAME_BYTES, 'x'), command, value));
    assert(parseCommand("MY_GROUPS", command, value) && command == ChatCommand::MyGroups && value.empty());
    assert(!parseCommand("MY_GROUPS ", command, value));
    assert(!parseCommand("MY_GROUPS extra", command, value));
    std::unique_ptr<ChatMessage> join = ChatProtocol::parseCommand("JOIN_GROUP 00042");
    const JoinGroupMessage* joinData = dynamic_cast<const JoinGroupMessage*>(join.get());
    assert(joinData && joinData->command() == ChatCommand::JoinGroup && joinData->groupId() == 42);
    assert(parseCommand("JOIN_GROUP 18446744073709551615", command, value));
    assert(!parseCommand("JOIN_GROUP", command, value));
    assert(!parseCommand("JOIN_GROUP ", command, value));
    assert(!parseCommand("JOIN_GROUP 0", command, value));
    assert(!parseCommand("JOIN_GROUP -1", command, value));
    assert(!parseCommand("JOIN_GROUP +1", command, value));
    assert(!parseCommand("JOIN_GROUP 1 2", command, value));
    assert(!parseCommand("JOIN_GROUP 18446744073709551616", command, value));
    assert(!parseCommand("JOIN_GROUP 000000000000000000001", command, value));

    ChatProtocol large;
    lines.clear();
    std::string valid = "MSG " + std::string(MAX_CHAT_MESSAGE_BYTES, 'x') + "\n";
    assert(large.append(valid.data(), valid.size() - 1, lines) && lines.empty());
    assert(large.append(valid.data() + valid.size() - 1, 1, lines) && lines.size() == 1);
    ChatProtocol oversized;
    std::string invalid = "PRIVATE " + std::string(MAX_CHAT_NICKNAME_BYTES, 'n') + " " + std::string(MAX_CHAT_MESSAGE_BYTES + 1, 'x');
    assert(!oversized.append(invalid.data(), invalid.size(), lines));
    return 0;
}
