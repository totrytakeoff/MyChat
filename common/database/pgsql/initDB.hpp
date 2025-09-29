#include <odb/core.hxx>
#include <string>
#include <odb/result.hxx>


// 使用SQL脚本创建表（推荐用于生产环境）
void create_tables_from_sql() {
    std::string sql = R"(
    CREATE TABLE IF NOT EXISTS user (
        uid TEXT PRIMARY KEY,
        account TEXT NOT NULL UNIQUE,
        nickname TEXT,
        avatar TEXT,
        gender INTEGER NOT NULL DEFAULT 0,
        signature TEXT,
        create_time INTEGER NOT NULL DEFAULT 0,
        last_login INTEGER NOT NULL DEFAULT 0,
        online INTEGER NOT NULL DEFAULT 0,
        phone_number TEXT,
        email TEXT,
        address TEXT,
        birthday TEXT,
        company TEXT,
        job_title TEXT,
        wxid TEXT,
        qqid TEXT,
        real_name TEXT,
        extra TEXT
    );
    )";

    odb::transaction t(db->begin());

    // 检查表是否存在
    odb::result<std::string> r(db->query<std::string>(
            "SELECT name FROM sqlite_master WHERE type='table' AND name='user'"));

    if (r.begin() == r.end()) {
        // 表不存在，创建表
        db->execute(sql);
        std::cout << "Table 'user' created successfully." << std::endl;
    }

    t.commit();
}
