// 仅供聊天协议联调使用的内存用户仓库，不访问真实数据库。
#include "../driver/mysql/UserRepository.h"
#include <unordered_map>

namespace {
std::unordered_map<std::string, User> accounts;
std::uint64_t nextUserId = 1;
}

UserResult UserRepository::findUser(const std::string& username, User& user)
{
    if (username == "dbfailure") return UserResult::DatabaseError;
    auto it = accounts.find(username);
    if (it == accounts.end()) return UserResult::NotFound;
    user = it->second;
    return UserResult::Success;
}

UserResult UserRepository::createUser(const std::string& username, const std::string& password)
{
    if (username == "dbfailure") return UserResult::DatabaseError;
    if (accounts.find(username) != accounts.end()) return UserResult::AlreadyExists;
    User user;
    user.id = nextUserId++;
    user.username = username;
    user.password = password;
    accounts.emplace(username, user);
    return UserResult::Success;
}
