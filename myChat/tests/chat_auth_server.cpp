// 仅供自动化联调使用；UserRepository 由 fake_user_repository.cpp 提供。
#include "../network/Kernel.h"
#include "../apps/chat/ChatHandler.h"
#include "../driver/mysql/MysqlConnection.h"

int main()
{
    MysqlConnection connection;
    UserRepository users(connection);
    ChatHandler handler(users);
    Kernel server(handler, 0);
    return server.run();
}
