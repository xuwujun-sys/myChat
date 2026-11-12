#include "MysqlConnection.h"
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/exception.h>
#include <cppconn/statement.h>
#include <utility>

MysqlConnection::MysqlConnection() = default;
MysqlConnection::~MysqlConnection() = default;

bool MysqlConnection::connect(const std::string& address, const std::string& username,
                              const std::string& password, const std::string& database)
{
    connection_.reset();
    last_error_.clear();
    try {
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        std::unique_ptr<sql::Connection> connection(driver->connect(address, username, password));
        connection->setSchema(database);
        std::unique_ptr<sql::Statement> statement(connection->createStatement());
        statement->execute("SET NAMES utf8mb4");
        connection_ = std::move(connection);
        return true;
    } catch (const sql::SQLException& error) {
        last_error_ = error.what();
        return false;
    }
}
