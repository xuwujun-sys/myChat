// 仅用于聊天协议测试，不访问真实数据库。
#include "../driver/mysql/GroupRepository.h"
#include <unordered_map>
#include <utility>

namespace {
std::vector<std::pair<std::uint64_t, Group>> memberships;
std::unordered_map<std::uint64_t, int> listModes;
}

GroupResult GroupRepository::createGroup(const std::string& name, std::uint64_t creatorId, std::uint64_t& groupId)
{
    if (creatorId == 0 || name == "test_db_error") return GroupResult::DatabaseError;
    if (name == "test_commit_unknown") { groupId = 100; return GroupResult::CommitUnknown; }
    if (name == "test_committed_lost") { groupId = 101; return GroupResult::SuccessConnectionLost; }
    if (name == "test_groups_failure") { listModes[creatorId] = 1; groupId = 102; return GroupResult::Success; }
    if (name == "test_groups_invalid") { listModes[creatorId] = 2; groupId = 103; return GroupResult::Success; }
    static std::uint64_t nextGroupId = 1;
    groupId = nextGroupId++;
    Group group;
    group.id = groupId;
    group.name = name;
    memberships.emplace_back(creatorId, group);
    return GroupResult::Success;
}

GroupResult GroupRepository::addMember(std::uint64_t groupId, std::uint64_t userId)
{
    if (groupId == UINT64_MAX) return GroupResult::DatabaseError;
    Group group;
    for (const auto& membership : memberships) {
        if (membership.second.id != groupId) continue;
        if (membership.first == userId) return GroupResult::AlreadyMember;
        group = membership.second;
    }
    if (group.id == 0) return GroupResult::NotFound;
    memberships.emplace_back(userId, group);
    return GroupResult::Success;
}

GroupResult GroupRepository::listGroups(std::uint64_t userId, std::vector<Group>& groups)
{
    if (listModes[userId] == 1) return GroupResult::DatabaseError;
    groups.clear();
    if (listModes[userId] == 2) {
        Group valid;
        valid.id = 1;
        valid.name = "valid";
        groups.push_back(valid);
        Group invalid;
        invalid.id = 2;
        invalid.name = "invalid\nname";
        groups.push_back(invalid);
        return GroupResult::Success;
    }
    for (const auto& membership : memberships) {
        if (membership.first == userId) groups.push_back(membership.second);
    }
    return GroupResult::Success;
}

GroupResult GroupRepository::listMemberIds(std::uint64_t groupId, std::vector<std::uint64_t>& userIds)
{
    if (groupId == UINT64_MAX) return GroupResult::DatabaseError;
    userIds.clear();
    for (const auto& membership : memberships) {
        if (membership.second.id == groupId) userIds.push_back(membership.first);
    }
    return GroupResult::Success;
}
