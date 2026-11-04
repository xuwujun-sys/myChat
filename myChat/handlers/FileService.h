#pragma once
#include <string>
#include "../core/UniqueFd.h"

// 当前小文件服务，返回 HTTP 状态码，文件数据通过输出参数返回。
class FileService {
public:
    static bool decodeFilename(const std::string& path, std::string& filename);
    // 打开文件并返回大小，文件描述符交给调用方持有。
    static int openFile(const std::string& path, UniqueFd& file, std::size_t& size);
    static bool readFile(const UniqueFd& file, std::size_t size, std::string& body);
    // filename 必须先通过 decodeFilename 校验。
    static int saveFile(const std::string& filename, const std::string& body);
};
