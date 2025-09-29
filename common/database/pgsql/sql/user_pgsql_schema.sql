-- PostgreSQL 用户表初始化脚本
-- 基于 services/odb/user.hpp 实体类定义

-- 设置时区（可选）
SET timezone = 'Asia/Shanghai';

-- 创建数据库（如果不存在）
-- CREATE DATABASE mychat;
-- \c mychat;

-- 创建扩展（如果需要）
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pgcrypto";

-- 删除已存在的表（用于开发环境，生产环境请谨慎使用）
-- 注意：删除表会同时删除所有数据，请确保在执行前已备份
DROP TABLE IF EXISTS user CASCADE;

-- 创建用户表
CREATE TABLE user (
    -- 主键字段
    uid VARCHAR(36) NOT NULL PRIMARY KEY DEFAULT uuid_generate_v4(),
    
    -- 基本信息
    account VARCHAR(255) NOT NULL UNIQUE,
    nickname VARCHAR(100),
    avatar VARCHAR(500),
    
    -- 性别枚举 (UNKNOWN=0, MALE=1, FEMALE=2, OTHER=3)
    gender SMALLINT NOT NULL DEFAULT 0 CHECK (gender IN (0, 1, 2, 3)),
    
    -- 个人信息
    signature TEXT,
    phone_number VARCHAR(20),
    email VARCHAR(255),
    address TEXT,
    birthday DATE,
    real_name VARCHAR(100),
    
    -- 职业信息
    company VARCHAR(100),
    job_title VARCHAR(100),
    
    -- 社交信息
    wxid VARCHAR(50),
    qqid VARCHAR(20),
    
    -- 系统字段
    create_time BIGINT NOT NULL DEFAULT extract(epoch from now()),
    last_login BIGINT NOT NULL DEFAULT 0,
    online BOOLEAN NOT NULL DEFAULT false,
    
    -- 扩展字段（JSON格式）
    extra TEXT,
    
    -- 索引
    CONSTRAINT chk_gender CHECK (gender IN (0, 1, 2, 3)),
    CONSTRAINT chk_phone_number CHECK (phone_number ~ '^[0-9+]*$' OR phone_number IS NULL),
    CONSTRAINT chk_email CHECK (email ~ '^[A-Za-z0-9._%-]+@[A-Za-z0-9.-]+[.][A-Za-z]+$' OR email IS NULL)
);

-- 创建索引
CREATE INDEX idx_user_account ON user(account);
CREATE INDEX idx_user_nickname ON user(nickname);
CREATE INDEX idx_user_gender ON user(gender);
CREATE INDEX idx_user_create_time ON user(create_time);
CREATE INDEX idx_user_last_login ON user(last_login);
CREATE INDEX idx_user_online ON user(online);
CREATE INDEX idx_user_phone_number ON user(phone_number);
CREATE INDEX idx_user_email ON user(email);

-- 创建复合索引
CREATE INDEX idx_user_gender_create_time ON user(gender, create_time);
CREATE INDEX idx_user_online_last_login ON user(online, last_login);

-- 创建触发器函数：自动更新最后登录时间
CREATE OR REPLACE FUNCTION update_last_login_time()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.online = true AND OLD.online = false THEN
        NEW.last_login = extract(epoch from now());
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- 创建触发器
CREATE TRIGGER trg_user_last_login
BEFORE UPDATE ON user
FOR EACH ROW
EXECUTE FUNCTION update_last_login_time();

-- 创建视图：用户基本信息视图
CREATE OR REPLACE VIEW user_basic_info AS
SELECT 
    uid,
    account,
    nickname,
    avatar,
    gender,
    signature,
    create_time,
    last_login,
    online,
    phone_number,
    email
FROM user;

-- 创建序列：用于可能的扩展需求
CREATE SEQUENCE IF NOT EXISTS user_id_seq
INCREMENT 1
START 1
MINVALUE 1
MAXVALUE 9223372036854775807
CACHE 1;

-- 插入示例数据（可选）
INSERT INTO user (
    account, 
    nickname, 
    avatar, 
    gender, 
    signature, 
    create_time, 
    online,
    phone_number,
    email,
    company,
    job_title
) VALUES (
    'admin', 
    '管理员', 
    '/avatars/admin.png', 
    1, 
    '系统管理员', 
    extract(epoch from now()) - 86400, 
    true,
    '13800138000',
    'admin@example.com',
    'MyChat公司',
    '系统架构师'
);

INSERT INTO user (
    account, 
    nickname, 
    avatar, 
    gender, 
    signature, 
    create_time, 
    online,
    phone_number,
    email,
    birthday,
    company,
    job_title
) VALUES (
    'user001', 
    '张三', 
    '/avatars/user001.png', 
    1, 
    '热爱编程的全栈开发者', 
    extract(epoch from now()) - 172800, 
    false,
    '13900139000',
    'zhangsan@example.com',
    '1990-01-01',
    '科技有限公司',
    '高级工程师'
);

INSERT INTO user (
    account, 
    nickname, 
    avatar, 
    gender, 
    signature, 
    create_time, 
    online,
    phone_number,
    email,
    birthday,
    company,
    job_title,
    extra
) VALUES (
    'user002', 
    '李四', 
    '/avatars/user002.png', 
    2, 
    'UI/UX设计师，追求完美', 
    extract(epoch from now()) - 259200, 
    true,
    '13700137000',
    'lisi@example.com',
    '1992-05-15',
    '设计工作室',
    '首席设计师',
    '{"preferences": {"theme": "dark", "language": "zh-CN"}, "skills": ["Photoshop", "Figma", "Sketch"]}'
);

-- 创建用户表统计信息函数
CREATE OR REPLACE FUNCTION get_user_stats()
RETURNS TABLE(
    total_users BIGINT,
    online_users BIGINT,
    male_users BIGINT,
    female_users BIGINT,
    other_users BIGINT,
    recent_users_24h BIGINT
) AS $$
BEGIN
    RETURN QUERY
    SELECT 
        COUNT(*)::BIGINT,
        COUNT(CASE WHEN online = true THEN 1 END)::BIGINT,
        COUNT(CASE WHEN gender = 1 THEN 1 END)::BIGINT,
        COUNT(CASE WHEN gender = 2 THEN 1 END)::BIGINT,
        COUNT(CASE WHEN gender = 3 THEN 1 END)::BIGINT,
        COUNT(CASE WHEN create_time > extract(epoch from now()) - 86400 THEN 1 END)::BIGINT
    FROM user;
END;
$$ LANGUAGE plpgsql;

-- 创建存储过程：批量插入用户
CREATE OR REPLACE PROCEDURE batch_insert_users(
    IN p_count INTEGER,
    IN p_start_index INTEGER
)
LANGUAGE plpgsql
AS $$
DECLARE
    i INTEGER;
    v_account VARCHAR(255);
    v_nickname VARCHAR(100);
    v_email VARCHAR(255);
    v_phone VARCHAR(20);
    v_create_time BIGINT;
BEGIN
    FOR i IN 1..p_count LOOP
        v_account := 'user_' || (p_start_index + i);
        v_nickname := '用户' || (p_start_index + i);
        v_email := 'user' || (p_start_index + i) || '@example.com';
        v_phone := '1' || LPAD((p_start_index + i)::TEXT, 10, '0');
        v_create_time := extract(epoch from now()) - (RANDOM() * 86400)::INTEGER;
        
        INSERT INTO user (
            account,
            nickname,
            email,
            phone_number,
            create_time,
            gender,
            company,
            job_title
        ) VALUES (
            v_account,
            v_nickname,
            v_email,
            v_phone,
            v_create_time,
            (RANDOM() * 3)::INTEGER + 1,
            '测试公司',
            '测试职位'
        );
    END LOOP;
    
    RAISE NOTICE '成功插入 % 个用户', p_count;
END;
$$;

-- 创建审计日志表（可选）
CREATE TABLE user_audit_log (
    id BIGSERIAL PRIMARY KEY,
    user_uid VARCHAR(36) NOT NULL,
    action VARCHAR(50) NOT NULL,
    old_data JSONB,
    new_data JSONB,
    action_time TIMESTAMP NOT NULL DEFAULT now(),
    ip_address INET,
    user_agent TEXT,
    
    FOREIGN KEY (user_uid) REFERENCES user(uid) ON DELETE CASCADE
);

-- 创建审计日志索引
CREATE INDEX idx_user_audit_log_user_uid ON user_audit_log(user_uid);
CREATE INDEX idx_user_audit_log_action_time ON user_audit_log(action_time);
CREATE INDEX idx_user_audit_log_action ON user_audit_log(action);

-- 创建触发器函数：记录用户变更
CREATE OR REPLACE FUNCTION log_user_changes()
RETURNS TRIGGER AS $$
BEGIN
    IF TG_OP = 'UPDATE' THEN
        INSERT INTO user_audit_log (user_uid, action, old_data, new_data, ip_address, user_agent)
        VALUES (
            NEW.uid,
            'UPDATE',
            row_to_json(OLD)::JSONB,
            row_to_json(NEW)::JSONB,
            NULL,
            NULL
        );
        RETURN NEW;
    ELSIF TG_OP = 'DELETE' THEN
        INSERT INTO user_audit_log (user_uid, action, old_data, ip_address, user_agent)
        VALUES (
            OLD.uid,
            'DELETE',
            row_to_json(OLD)::JSONB,
            NULL,
            NULL
        );
        RETURN OLD;
    ELSIF TG_OP = 'INSERT' THEN
        INSERT INTO user_audit_log (user_uid, action, new_data, ip_address, user_agent)
        VALUES (
            NEW.uid,
            'INSERT',
            row_to_json(NEW)::JSONB,
            NULL,
            NULL
        );
        RETURN NEW;
    END IF;
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

-- 为用户表创建审计触发器
CREATE TRIGGER trg_user_audit_log
AFTER INSERT OR UPDATE OR DELETE ON user
FOR EACH ROW
EXECUTE FUNCTION log_user_changes();

-- 创建权限管理（可选）
-- 创建角色
CREATE ROLE IF NOT EXISTS app_user WITH LOGIN PASSWORD 'secure_password_123';
CREATE ROLE IF NOT EXISTS app_admin WITH LOGIN PASSWORD 'admin_password_123' INHERIT app_user;

-- 授予基本权限
GRANT CONNECT ON DATABASE mychat TO app_user;
GRANT USAGE ON SCHEMA public TO app_user;
GRANT SELECT, INSERT, UPDATE, DELETE ON user TO app_user;
GRANT SELECT ON user_audit_log TO app_user;

-- 授予管理员权限
GRANT SELECT, INSERT, UPDATE, DELETE ON user_audit_log TO app_admin;
GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO app_admin;

-- 创建只读用户角色
CREATE ROLE IF NOT EXISTS app_readonly WITH LOGIN PASSWORD 'readonly_password_123';
GRANT CONNECT ON DATABASE mychat TO app_readonly;
GRANT USAGE ON SCHEMA public TO app_readonly;
GRANT SELECT ON user TO app_readonly;
GRANT SELECT ON user_audit_log TO app_readonly;

-- 输出创建完成信息
DO $$
BEGIN
    RAISE NOTICE 'PostgreSQL 用户表创建完成！';
    RAISE NOTICE '表结构：';
    RAISE NOTICE '  - 主键: uid (UUID)';
    RAISE NOTICE '  - 唯一约束: account';
    RAISE NOTICE '  - 索引: account, nickname, gender, create_time, last_login, online';
    RAISE NOTICE '  - 触发器: 自动更新最后登录时间';
    RAISE NOTICE '  - 视图: user_basic_info';
    RAISE NOTICE '  - 审计: user_audit_log';
    RAISE NOTICE '  - 示例数据: 已插入3个测试用户';
END $$;
