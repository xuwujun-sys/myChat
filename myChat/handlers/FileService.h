#pragma once
#include <string>
#include "../core/UniqueFd.h"

// 文件操作返回 HTTP 状态码，文件数据通过输出参数返回。
class FileService {
public:
    static bool decodeFilename(const std::string& path, std::string& filename);
    // 打开文件并返回大小，文件描述符交给调用方持有。
    static int openFile(const std::string& path, UniqueFd& file, std::size_t& size);
    // 读取最多 size 字节，返回实际读取的字节数；失败返回 -1。
    static ssize_t readFile(const UniqueFd& file, char* buffer, std::size_t size);
};

// 上传过程中持有临时文件，未完成时析构自动删除。
class UploadFile {
public:
    ~UploadFile();
    int open(const std::string& filename);
    bool write(const char* data, std::size_t size);
    int finish();
private:
    UniqueFd directory_;
    UniqueFd file_;
    std::string filename_;
    std::string temporary_;
};
