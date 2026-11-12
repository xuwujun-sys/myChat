#pragma once
#include <memory>
#include <string>

namespace sql { class Connection; }

// 独占一个数据库连接，对象销毁时自动释放。
class MysqlConnection {
public:
    MysqlConnection();
    ~MysqlConnection();

    bool connect(const std::string& address, const std::string& username,
                 const std::string& password, const std::string& database);
    sql::Connection* get() const { return connection_.get(); }
    const std::string& lastError() const { return last_error_; }

private:
    std::unique_ptr<sql::Connection> connection_;
    std::string last_error_;
};
