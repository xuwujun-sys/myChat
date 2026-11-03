#include "core/Kernel.h"
#include "handlers/HttpHandler.h"

int main()
{
    HttpHandler handler;
    Kernel server(handler);
    return server.run();
}
