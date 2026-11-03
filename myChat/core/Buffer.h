#pragma once
#include <cstddef>
#include <string>

// 保存等待处理的数据。
class Buffer {
public:
    // 追加数据。
    void append(const char* data, std::size_t len);
    // 获取可读数据的起始地址和长度。
    const char* data() const;
    std::size_t size() const;
    bool empty() const;
    // 消费前面的 len 字节，len 不能超过可读数据长度。
    void consume(std::size_t len);

private:
    std::string data_;
    std::size_t read_pos_ = 0; // 当前可读数据的起始位置。
};
