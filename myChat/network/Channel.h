#pragma once

// 通道基类：持有文件描述符并负责关闭。
class Channel {
public:
    virtual ~Channel();
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    int getFd() const { return fd_; }

protected:
    explicit Channel(int fd = -1) : fd_(fd) {}
    // 关闭文件描述符并重置状态，可重复调用。
    void closeFd();
    int fd_;
};
