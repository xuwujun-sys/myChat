#include "GroupRepository.h"
#include "MysqlConnection.h"
#include <cppconn/connection.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <memory>
#include <utility>

GroupResult GroupRepository::createGroup(const std::string& name, std::uint64_t creatorId, std::uint64_t& groupId)
{
    last_error_.clear();
    sql::Connection* connection = connection_.get();
    if (!connection) {
        last_error_ = "Database is not connected";
        return GroupResult::DatabaseError;
    }
    bool transactionStarted = false;
    bool commitStarted = false;
    bool committed = false;
    try {
        if (!connection->getAutoCommit()) {
            last_error_ = "Connection already has an active transaction";
            return GroupResult::DatabaseError;
        }
        connection->setAutoCommit(false);
        transactionStarted = true;
        std::unique_ptr<sql::PreparedStatement> group(connection->prepareStatement(
            "INSERT INTO chat_groups (name) VALUES (?)"));
        group->setString(1, name);
        group->executeUpdate();
        std::unique_ptr<sql::Statement> query(connection->createStatement());
        std::unique_ptr<sql::ResultSet> result(query->executeQuery("SELECT LAST_INSERT_ID()"));
        if (!result->next()) {
            connection->rollback();
            connection->setAutoCommit(true);
            last_error_ = "Could not retrieve group ID";
            return GroupResult::DatabaseError;
        }
        std::uint64_t id = result->getUInt64(1);
        std::unique_ptr<sql::PreparedStatement> member(connection->prepareStatement(
            "INSERT INTO group_members (group_id, user_id) VALUES (?, ?)"));
        member->setUInt64(1, id);
        member->setUInt64(2, creatorId);
        member->executeUpdate();
        groupId = id;
        commitStarted = true;
        connection->commit();
        committed = true;
        connection->setAutoCommit(true);
        return GroupResult::Success;
    } catch (const sql::SQLException& error) {
        last_error_ = error.what();
        if (commitStarted) {
            connection_.disconnect();
            return committed ? GroupResult::SuccessConnectionLost : GroupResult::CommitUnknown;
        }
        if (transactionStarted) {
            try {
                connection->rollback();
                connection->setAutoCommit(true);
            } catch (const sql::SQLException& cleanupError) {
                last_error_ += "; transaction cleanup failed: ";
                last_error_ += cleanupError.what();
                connection_.disconnect();
                return GroupResult::DatabaseError;
            }
        } else {
            connection_.disconnect();
        }
        return error.getErrorCode() == 1452 ? GroupResult::NotFound : GroupResult::DatabaseError;
    }
}

GroupResult GroupRepository::addMember(std::uint64_t groupId, std::uint64_t userId)
{
    last_error_.clear();
    if (!connection_.get()) {
        last_error_ = "Database is not connected";
        return GroupResult::DatabaseError;
    }
    try {
        std::unique_ptr<sql::PreparedStatement> statement(connection_.get()->prepareStatement(
            "INSERT INTO group_members (group_id, user_id) VALUES (?, ?)"));
        statement->setUInt64(1, groupId);
        statement->setUInt64(2, userId);
        statement->executeUpdate();
        return GroupResult::Success;
    } catch (const sql::SQLException& error) {
        if (error.getErrorCode() == 1062) return GroupResult::AlreadyMember;
        if (error.getErrorCode() == 1452) return GroupResult::NotFound;
        last_error_ = error.what();
        return GroupResult::DatabaseError;
    }
}

GroupResult GroupRepository::listGroups(std::uint64_t userId, std::vector<Group>& groups)
{
    last_error_.clear();
    if (!connection_.get()) {
        last_error_ = "Database is not connected";
        return GroupResult::DatabaseError;
    }
    try {
        std::unique_ptr<sql::PreparedStatement> statement(connection_.get()->prepareStatement(
            "SELECT g.id, g.name FROM chat_groups g JOIN group_members m ON g.id = m.group_id "
            "WHERE m.user_id = ? ORDER BY g.id"));
        statement->setUInt64(1, userId);
        std::unique_ptr<sql::ResultSet> result(statement->executeQuery());
        std::vector<Group> found;
        while (result->next()) {
            Group group;
            group.id = result->getUInt64(1);
            group.name = result->getString(2).asStdString();
            found.push_back(std::move(group));
        }
        groups = std::move(found);
        return GroupResult::Success;
    } catch (const sql::SQLException& error) {
        last_error_ = error.what();
        return GroupResult::DatabaseError;
    }
}

GroupResult GroupRepository::listMemberIds(std::uint64_t groupId, std::vector<std::uint64_t>& userIds)
{
    last_error_.clear();
    if (!connection_.get()) {
        last_error_ = "Database is not connected";
        return GroupResult::DatabaseError;
    }
    try {
        std::unique_ptr<sql::PreparedStatement> statement(connection_.get()->prepareStatement(
            "SELECT user_id FROM group_members WHERE group_id = ?"));
        statement->setUInt64(1, groupId);
        std::unique_ptr<sql::ResultSet> result(statement->executeQuery());
        std::vector<std::uint64_t> found;
        while (result->next()) found.push_back(result->getUInt64(1));
        userIds = std::move(found);
        return GroupResult::Success;
    } catch (const sql::SQLException& error) {
        last_error_ = error.what();
        return GroupResult::DatabaseError;
    }
}
