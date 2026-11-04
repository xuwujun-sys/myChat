#include "FileService.h"
#include "../core/UniqueFd.h"
#include "../config/config.h"
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>

// 将 URL 中的文件名解码，并限制为单个安全的路径分量。
bool FileService::decodeFilename(const std::string& path, std::string& filename)
{
    for (std::size_t i = 0; i < path.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(path[i]);
        if (c == '%') {
            if (i + 2 >= path.size()) return false;
            int value = 0;
            for (int j = 0; j < 2; ++j) {
                unsigned char hex = static_cast<unsigned char>(path[++i]);
                if (hex >= '0' && hex <= '9') value = value * 16 + hex - '0';
                else if (hex >= 'a' && hex <= 'f') value = value * 16 + hex - 'a' + 10;
                else if (hex >= 'A' && hex <= 'F') value = value * 16 + hex - 'A' + 10;
                else return false;
            }
            c = static_cast<unsigned char>(value);
        } else if (c == '?' || c == '#') {
            return false;
        }
        if (c < 32 || c == 127 || c == '/' || c == '\\') return false;
        filename.push_back(static_cast<char>(c));
    }
    // 防止客户端下载正在写入的临时文件。
    return !filename.empty() && filename != "." && filename != ".." &&
        filename.compare(0, 8, ".upload-") != 0;
}

// 打开共享目录中的文件，返回文件大小。
int FileService::openFile(const std::string& path, UniqueFd& file, std::size_t& size)
{
    std::string filename;
    if (!decodeFilename(path, filename)) return 400;
    UniqueFd directory(open(HTTP_FILES_DIR, O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (directory.get() < 0) return 500;
    file = UniqueFd(openat(directory.get(), filename.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
    int openError = errno;
    directory.close();
    if (file.get() < 0) return openError == ENOENT || openError == ELOOP ? 404 : 500;

    struct stat info{};
    if (fstat(file.get(), &info) < 0) return 500;
    if (!S_ISREG(info.st_mode)) return 404;
    if (info.st_size < 0 || static_cast<unsigned long long>(info.st_size) > std::numeric_limits<std::size_t>::max()) return 500;
    size = static_cast<std::size_t>(info.st_size);
    return 200;
}

// 读取指定数量的文件内容。
bool FileService::readFile(const UniqueFd& file, std::size_t size, std::string& body)
{
    std::size_t remaining = size;
    char buffer[4096];
    while (remaining > 0) {
        std::size_t amount = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        ssize_t count = read(file.get(), buffer, amount);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        body.append(buffer, static_cast<std::size_t>(count));
        remaining -= static_cast<std::size_t>(count);
    }
    return true;
}

UploadFile::~UploadFile()
{
    file_.close();
    if (directory_.get() >= 0 && !temporary_.empty()) {
        unlinkat(directory_.get(), temporary_.c_str(), 0);
    }
}

// 创建临时文件，filename 必须先通过 decodeFilename 校验。
int UploadFile::open(const std::string& filename)
{
    filename_ = filename;
    directory_ = UniqueFd(::open(HTTP_FILES_DIR, O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (directory_.get() < 0) return 500;
    static unsigned long nextId = 0;
    for (int attempts = 0; attempts < 100; ++attempts) {
        std::string name = ".upload-" + std::to_string(getpid()) + "-" + std::to_string(nextId++);
        if (name == filename_) continue;
        file_ = UniqueFd(openat(directory_.get(), name.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600));
        if (file_.get() >= 0) { temporary_ = name; return 200; }
        if (errno != EEXIST) break;
    }
    return 500;
}

// 将接收到的一段数据完整写入临时文件。
bool UploadFile::write(const char* data, std::size_t size)
{
    std::size_t written = 0;
    while (written < size) {
        ssize_t count = ::write(file_.get(), data + written, size - written);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        written += static_cast<std::size_t>(count);
    }
    return true;
}

// 全部收到后创建目标文件；同名文件不会被覆盖。
int UploadFile::finish()
{
    if (file_.close() < 0) return 500;
    if (linkat(directory_.get(), temporary_.c_str(), directory_.get(), filename_.c_str(), 0) < 0)
        return errno == EEXIST ? 409 : 500;
    if (unlinkat(directory_.get(), temporary_.c_str(), 0) == 0) temporary_.clear();
    return 201;
}
