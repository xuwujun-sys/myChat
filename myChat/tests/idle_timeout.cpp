#include "../network/Kernel.h"
#include "../apps/echo/EchoHandler.h"
#include <cassert>
#include <string>

// 无参数时检查超时判断；传入秒数或 default 时启动回显服务器供联调。
int main(int argc, char* argv[])
{
    EchoHandler handler;
    if (argc == 2) {
        if (std::string(argv[1]) == "default") {
            Kernel server(handler);
            return server.run();
        }
        Kernel server(handler, std::stoi(argv[1]));
        return server.run();
    }
    TcpChannel channel(-1);
    auto future = std::chrono::steady_clock::now() + std::chrono::seconds(61);
    assert(channel.isIdle(future, 60));
    assert(!channel.isIdle(future, 120));
    assert(!channel.isIdle(future, 0));
    Kernel invalid(handler, -1);
    assert(invalid.run() != 0);
    return 0;
}
