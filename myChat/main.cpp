#include "network/Kernel.h"
#include "apps/chat/ChatHandler.h"
#include "driver/mysql/MysqlConnection.h"
#include <cstdlib>
#include <iostream>

int main()
{
    const char* username = std::getenv("CHAT_DB_USER");
    const char* password = std::getenv("CHAT_DB_PASSWORD");
    if (!username || !password) {
        std::cerr << "Set CHAT_DB_USER and CHAT_DB_PASSWORD" << std::endl;
        return 1;
    }
    const char* host = std::getenv("CHAT_DB_HOST");
    const char* database = std::getenv("CHAT_DB_NAME");
    MysqlConnection connection;
    if (!connection.connect(host ? host : "tcp://127.0.0.1:3306",
                            username, password, database ? database : "chatroom")) {
        std::cerr << "Database connection failed: " << connection.lastError() << std::endl;
        return 1;
    }
    UserRepository users(connection);
    ChatHandler handler(users);
    Kernel server(handler, 0); // 聊天连接不启用空闲超时。
    return server.run();
}
