#pragma once
#include <cstdint>
#include <string>
#include <vector>

class MysqlConnection;

struct Group {
    std::uint64_t id = 0;
    std::string name;
};

enum class GroupResult { Success, AlreadyMember, NotFound, DatabaseError };

// 管理群组和群成员，不负责在线状态。
class GroupRepository {
public:
    explicit GroupRepository(MysqlConnection& connection) : connection_(connection) {}

    // 创建群并添加创建者；成功时返回群 ID。
    GroupResult createGroup(const std::string& name, std::uint64_t creatorId, std::uint64_t& groupId);
    GroupResult addMember(std::uint64_t groupId, std::uint64_t userId);
    // 查询成功时更新列表；未加入任何群时返回空列表。
    GroupResult listGroups(std::uint64_t userId, std::vector<Group>& groups);
    const std::string& lastError() const { return last_error_; }

private:
    MysqlConnection& connection_;
    std::string last_error_;
};
