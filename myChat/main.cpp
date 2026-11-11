#include "network/Kernel.h"
#include "apps/chat/ChatHandler.h"

int main()
{
    ChatHandler handler;
    Kernel server(handler, 0); // 聊天连接不启用空闲超时。
    return server.run();
}
