#include "network/Kernel.h"
#include "apps/http/HttpHandler.h"

int main()
{
    HttpHandler handler;
    Kernel server(handler);
    return server.run();
}
