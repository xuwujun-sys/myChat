// 仅供聊天协议联调使用的内存用户仓库，不访问真实数据库。
#include "../driver/mysql/UserRepository.h"
#include <unordered_map>

namespace {
std::unordered_map<std::string, std::string> accounts;
}

UserResult UserRepository::findUser(const std::string& username, User& user)
{
    if (username == "dbfailure") return UserResult::DatabaseError;
    auto it = accounts.find(username);
    if (it == accounts.end()) return UserResult::NotFound;
    user.username = it->first;
    user.password = it->second;
    return UserResult::Success;
}

UserResult UserRepository::createUser(const std::string& username, const std::string& password)
{
    if (username == "dbfailure") return UserResult::DatabaseError;
    return accounts.emplace(username, password).second ? UserResult::Success : UserResult::AlreadyExists;
}
