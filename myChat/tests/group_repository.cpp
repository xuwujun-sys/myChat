// 只在隔离测试数据库中运行，不连接项目使用的数据库。
#include "../driver/mysql/GroupRepository.h"
#include "../driver/mysql/MysqlConnection.h"
#include <cppconn/connection.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

int main()
{
    MysqlConnection disconnected;
    GroupRepository unavailable(disconnected);
    std::uint64_t id = 99;
    std::vector<Group> groups;
    assert(unavailable.createGroup("group", 1, id) == GroupResult::DatabaseError && id == 99);
    assert(unavailable.addMember(1, 1) == GroupResult::DatabaseError);
    assert(unavailable.listGroups(1, groups) == GroupResult::DatabaseError);

    const char* host = std::getenv("GROUP_TEST_HOST");
    const char* database = std::getenv("GROUP_TEST_DATABASE");
    if (!host || !database || std::string(database).find("codex_group_test_") != 0) {
        std::cout << "Disconnected checks passed; isolated database test skipped\n";
        return 0;
    }
    MysqlConnection connection;
    assert(connection.connect(host, "root", "", database));
    GroupRepository repository(connection);
    assert(repository.createGroup("Group 'one'", 1, id) == GroupResult::Success && id > 0);
    assert(repository.listGroups(1, groups) == GroupResult::Success && groups.size() == 1);
    assert(groups[0].id == id && groups[0].name == "Group 'one'");
    assert(repository.addMember(id, 1) == GroupResult::AlreadyMember);
    assert(repository.addMember(id, 2) == GroupResult::Success);
    assert(repository.listGroups(2, groups) == GroupResult::Success && groups.size() == 1 && groups[0].id == id);
    assert(repository.addMember(id, 999999) == GroupResult::NotFound);
    assert(repository.addMember(999999, 1) == GroupResult::NotFound);
    std::uint64_t failedId = 99;
    assert(repository.createGroup("must rollback", 999999, failedId) == GroupResult::NotFound && failedId == 99);
    assert(connection.get()->getAutoCommit());
    std::unique_ptr<sql::Statement> statement(connection.get()->createStatement());
    std::unique_ptr<sql::ResultSet> count(statement->executeQuery("SELECT COUNT(*) FROM chat_groups"));
    assert(count->next() && count->getUInt64(1) == 1);
    assert(repository.listGroups(1, groups) == GroupResult::Success && groups.size() == 1);
    connection.get()->setAutoCommit(false);
    assert(repository.createGroup("nested", 1, failedId) == GroupResult::DatabaseError && failedId == 99);
    assert(!connection.get()->getAutoCommit());
    connection.get()->rollback();
    connection.get()->setAutoCommit(true);
    assert(repository.listGroups(999999, groups) == GroupResult::Success && groups.empty());
    std::cout << "Group repository isolated database checks passed\n";
}
