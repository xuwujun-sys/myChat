#include "Kernel.h"
#include "EchoHandler.h"

int main()
{
    EchoHandler handler;
    Kernel server(handler);
    return server.run();
}
