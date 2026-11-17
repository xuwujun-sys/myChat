// 仅配合隔离的 MySQL 和测试故障注入运行。
// 编译时使用 group_commit_fault.cpp 代替 GroupRepository.cpp。
#include "../driver/mysql/GroupRepository.h"
#include "../driver/mysql/MysqlConnection.h"
#include <cassert>
#include <cstdlib>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    assert(argc == 2);
    const char* database = std::getenv("GROUP_TEST_DATABASE");
    const char* host = std::getenv("GROUP_TEST_HOST");
    assert(database && std::string(database).find("codex_group_test_") == 0 && host);
    const std::string mode = argv[1];
    assert(mode == "restore" || mode == "commit");

    MysqlConnection connection;
    assert(connection.connect(host, "root", "", database));
    GroupRepository repository(connection);
    std::uint64_t groupId = 0;
    GroupResult result = repository.createGroup("fault-" + mode, 1, groupId);
    if (mode == "restore") assert(result == GroupResult::SuccessConnectionLost);
    else assert(result == GroupResult::CommitUnknown);
    assert(groupId != 0 && !repository.lastError().empty() && connection.get() == nullptr);

    // 驱动故障在实际 COMMIT 后触发，独立连接确认数据库状态。
    MysqlConnection verification;
    assert(verification.connect(host, "root", "", database));
    GroupRepository reader(verification);
    std::vector<Group> groups;
    assert(reader.listGroups(1, groups) == GroupResult::Success);
    bool found = false;
    for (const Group& group : groups) {
        if (group.id == groupId && group.name == "fault-" + mode) found = true;
    }
    assert(found);
}
