#pragma once
#include "Buffer.h"
#include "Channel.h"
#include <functional>

class Handler;

// 封装客户端 socket 的接收和发送逻辑。
class TcpChannel : public Channel {
public:
    // 一个通道对应一个客户端，通道销毁时关闭该 fd。
    explicit TcpChannel(int fd, std::function<bool(const TcpChannel&)> notifySend = {});
    ~TcpChannel() override = default;

    // 从 fd 接收数据并交给处理器；发生需要关闭连接的错误时返回 false。
    bool receiveData(Handler& handler);
    // 将数据加入待发送缓冲区；超过上限或通知失败时返回 false。
    bool queueSend(const char* data, std::size_t size);
    // 发送 pending 中的数据；暂时无法全部发送时保留剩余数据。
    // 成功或暂时不可写返回 true，发生需要关闭连接的错误时返回 false。
    bool sendData();
    // 发送缓冲区清空后关闭连接，不再向处理器交付新数据。
    void closeAfterSend() { close_after_send_ = true; }

    // 查询连接状态，收发方法负责更新状态。
    bool hasPendingData() const { return !pending_.empty(); }
    bool isReadClosed() const { return read_closed_; }
    bool shouldCloseAfterSend() const { return close_after_send_; }

private:
    Buffer pending_; // 等待发送的数据。
    bool read_closed_ = false; // 对端已结束发送，但仍可能等待接收回显。
    bool close_after_send_ = false;
    std::function<bool(const TcpChannel&)> notify_send_; // 通知事件循环关注可写事件。
};
