#include "UserRepository.h"
#include "MysqlConnection.h"
#include <cppconn/connection.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <memory>
#include <utility>

UserResult UserRepository::findUser(const std::string& username, User& user)
{
    last_error_.clear();
    if (!connection_.get()) {
        last_error_ = "Database is not connected";
        return UserResult::DatabaseError;
    }
    try {
        std::unique_ptr<sql::PreparedStatement> statement(connection_.get()->prepareStatement(
            "SELECT id, username, password FROM users WHERE username = ?"));
        statement->setString(1, username);
        std::unique_ptr<sql::ResultSet> result(statement->executeQuery());
        if (!result->next()) return UserResult::NotFound;
        User found;
        found.id = result->getUInt64(1);
        found.username = result->getString(2).asStdString();
        found.password = result->getString(3).asStdString();
        user = std::move(found);
        return UserResult::Success;
    } catch (const sql::SQLException& error) {
        last_error_ = error.what();
        return UserResult::DatabaseError;
    }
}

UserResult UserRepository::createUser(const std::string& username, const std::string& password)
{
    last_error_.clear();
    if (!connection_.get()) {
        last_error_ = "Database is not connected";
        return UserResult::DatabaseError;
    }
    try {
        std::unique_ptr<sql::PreparedStatement> statement(connection_.get()->prepareStatement(
            "INSERT INTO users (username, password) VALUES (?, ?)"));
        statement->setString(1, username);
        statement->setString(2, password);
        statement->executeUpdate();
        return UserResult::Success;
    } catch (const sql::SQLException& error) {
        if (error.getErrorCode() == 1062) return UserResult::AlreadyExists;
        last_error_ = error.what();
        return UserResult::DatabaseError;
    }
}
