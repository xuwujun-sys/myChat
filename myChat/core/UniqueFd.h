#pragma once
#include <unistd.h>

// 独占文件描述符，对象销毁时自动关闭。
class UniqueFd {
public:
    explicit UniqueFd(int fd = -1) : fd_(fd) {}
    ~UniqueFd() { close(); }
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd(UniqueFd&& other) noexcept : fd_(other.release()) {}
    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) { close(); fd_ = other.release(); }
        return *this;
    }
    int get() const { return fd_; }
    int release() { int fd = fd_; fd_ = -1; return fd; }
    int close() { return fd_ < 0 ? 0 : ::close(release()); }
private:
    int fd_;
};
