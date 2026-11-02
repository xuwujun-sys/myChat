#pragma once
#include <string>

// 保存一个客户端连接在收发过程中需要的状态。
struct Client {
    std::string pending; // 等待回传的数据
    bool read_closed = false; // 对端已结束发送，但仍可能等待接收回显。
};


// 封装客户端 socket 的接收和发送逻辑。
class TcpChannel {
public:
    // 从 fd 接收数据并追加到 pending；发生需要关闭连接的错误时返回 false。
    static bool receiveData(int fd, Client& client);
    // 发送 pending 中的数据；暂时无法全部发送时保留剩余数据。
    // 成功或暂时不可写返回 true，发生需要关闭连接的错误时返回 false。
    static bool sendData(int fd, Client& client);
};
