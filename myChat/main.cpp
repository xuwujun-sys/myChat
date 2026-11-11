#include "network/Kernel.h"
#include "apps/chat/ChatHandler.h"

int main()
{
    ChatHandler handler;
    Kernel server(handler);
    return server.run();
}
