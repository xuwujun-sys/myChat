#include "../driver/mysql/MysqlConnection.h"
#include "../driver/mysql/UserRepository.h"
#include <cassert>
#include <type_traits>

int main()
{
    static_assert(!std::is_copy_constructible<MysqlConnection>::value, "connection must be unique");
    MysqlConnection connection;
    UserRepository users(connection);
    User user;
    assert(users.findUser("alice", user) == UserResult::DatabaseError);
    assert(!users.lastError().empty());
    assert(users.createUser("alice", "hash") == UserResult::DatabaseError);

    // 本机端口 1 没有 MySQL 服务，验证连接失败时不会留下连接对象。
    assert(!connection.connect("tcp://127.0.0.1:1", "test", "test", "chatroom"));
    assert(connection.get() == nullptr);
    assert(!connection.lastError().empty());
    assert(users.findUser("alice", user) == UserResult::DatabaseError);
    return 0;
}
