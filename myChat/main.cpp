#include "Kernel.h"
#include "HttpHandler.h"

int main()
{
    HttpHandler handler;
    Kernel server(handler);
    return server.run();
}
