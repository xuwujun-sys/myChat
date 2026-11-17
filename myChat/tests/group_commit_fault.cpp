// 仅供测试：在真实数据库调用返回后注入异常，验证事务结果处理。
#include "../driver/mysql/GroupRepository.h"
#include "../driver/mysql/MysqlConnection.h"
#include <cppconn/connection.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>

namespace {
bool restoreFailurePending = false;

void afterCommit()
{
    const char* mode = std::getenv("GROUP_TEST_FAULT");
    if (mode && std::strcmp(mode, "commit") == 0) throw sql::SQLException("injected commit response failure");
    restoreFailurePending = mode && std::strcmp(mode, "restore") == 0;
}

void afterSetAutoCommit(bool enabled)
{
    if (enabled && restoreFailurePending) {
        restoreFailurePending = false;
        throw sql::SQLException("injected autocommit restoration failure");
    }
}
}

#define commit() commit(); afterCommit()
#define setAutoCommit(enabled) setAutoCommit(enabled); afterSetAutoCommit(enabled)
#include "../driver/mysql/GroupRepository.cpp"
#undef setAutoCommit
#undef commit
