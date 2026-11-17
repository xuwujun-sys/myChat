// 仅用于聊天协议测试，不访问真实数据库。
#include "../driver/mysql/GroupRepository.h"

GroupResult GroupRepository::createGroup(const std::string& name, std::uint64_t creatorId, std::uint64_t& groupId)
{
    if (creatorId == 0 || name == "test_db_error") return GroupResult::DatabaseError;
    if (name == "test_commit_unknown") { groupId = 100; return GroupResult::CommitUnknown; }
    if (name == "test_committed_lost") { groupId = 101; return GroupResult::SuccessConnectionLost; }
    static std::uint64_t nextGroupId = 1;
    groupId = nextGroupId++;
    return GroupResult::Success;
}
