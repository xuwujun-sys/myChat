#pragma once
#include <string>
#include "../../network/UniqueFd.h"

// 文件操作返回 HTTP 状态码，文件数据通过输出参数返回。
class FileService {
public:
    static bool decodeFilename(const std::string& path, std::string& filename);
    // 打开文件并返回大小，文件描述符交给调用方持有。
    static int openFile(const std::string& path, UniqueFd& file, std::size_t& size);
    static bool readFile(const UniqueFd& file, std::size_t size, std::string& body);
};

// 上传期间持有临时文件；未完成时析构会删除临时文件。
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
