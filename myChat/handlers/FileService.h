#pragma once
#include <string>

// 当前小文件服务，返回 HTTP 状态码，文件数据通过输出参数返回。
class FileService {
public:
    static bool decodeFilename(const std::string& path, std::string& filename);
    static int readFile(const std::string& path, std::string& body);
    // filename 必须先通过 decodeFilename 校验。
    static int saveFile(const std::string& filename, const std::string& body);
};
