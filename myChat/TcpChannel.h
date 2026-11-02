#pragma once
#include <string>

// 封装客户端 socket 的接收和发送逻辑。
class TcpChannel {
public:
    // 一个通道对应一个客户端，通道销毁时关闭该 fd。
    explicit TcpChannel(int fd);
    ~TcpChannel();
    TcpChannel(const TcpChannel&) = delete;
    TcpChannel& operator=(const TcpChannel&) = delete;

    // 从 fd 接收数据并追加到 pending；发生需要关闭连接的错误时返回 false。
    bool receiveData();
    // 发送 pending 中的数据；暂时无法全部发送时保留剩余数据。
    // 成功或暂时不可写返回 true，发生需要关闭连接的错误时返回 false。
    bool sendData();

    // 查询连接状态，收发方法负责更新状态。
    bool hasPendingData() const { return !pending_.empty(); }
    bool isReadClosed() const { return read_closed_; }

private:
    int fd_;
    std::string pending_; // 等待发送的数据。
    bool read_closed_ = false; // 对端已结束发送，但仍可能等待接收回显。
};
