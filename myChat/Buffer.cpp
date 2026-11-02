#include "Buffer.h"
#include <cassert>

void Buffer::append(const char* data, std::size_t len)
{
    // 已消费的空间较多时整理，避免缓冲区持续增长。
    if (read_pos_ > 0 && read_pos_ >= size()) {
        data_.erase(0, read_pos_);
        read_pos_ = 0;
    }
    data_.append(data, len);
}

const char* Buffer::data() const
{
    return data_.data() + read_pos_;
}

std::size_t Buffer::size() const
{
    return data_.size() - read_pos_;
}

bool Buffer::empty() const
{
    return size() == 0;
}

void Buffer::consume(std::size_t len)
{
    assert(len <= size());
    read_pos_ += len;
    if (read_pos_ == data_.size()) {
        data_.clear();
        read_pos_ = 0;
    }
}
