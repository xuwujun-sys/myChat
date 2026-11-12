#pragma once
#include <cstdint>
#include <string>

class MysqlConnection;

struct User {
    std::uint64_t id = 0;
    std::string username;
    std::string password_hash;
};

enum class UserResult { Success, NotFound, AlreadyExists, DatabaseError };

// 只负责 users 表的查询和写入；密码哈希由调用方生成。
class UserRepository {
public:
    explicit UserRepository(MysqlConnection& connection) : connection_(connection) {}
    // 返回 Success 时才更新 user。
    UserResult findUser(const std::string& username, User& user);
    UserResult createUser(const std::string& username, const std::string& passwordHash);
    const std::string& lastError() const { return last_error_; }

private:
    MysqlConnection& connection_;
    std::string last_error_;
};
