#include "../../common/utils/config_mgr.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using namespace std;
using namespace im::utils;
using json = nlohmann::json;

void print_separator(const string& title) {
    cout << "\n" << string(50, '=') << endl;
    cout << "  " << title << endl;
    cout << string(50, '=') << endl;
}

void test(){
        try {
            ConfigManager config_mgr("../config.json");

        print_separator("测试原有的get方法");
        
        // 测试字符串获取
        cout << "redis.host (string): " << config_mgr.get("redis.host") << endl;
        cout << "mysql.host (string): " << config_mgr.get("mysql.host") << endl;
        cout << "mysql.user (string): " << config_mgr.get("mysql.user") << endl;
        cout << "mysql.password (string): '" << config_mgr.get("mysql.password") << "'" << endl;
        cout << "host (string): " << config_mgr.get("host") << endl;
        
        // 测试不存在的键
        cout << "non_existent_key (string): '" << config_mgr.get("non_existent_key") << "'" << endl;
        cout << "host.port (string): '" << config_mgr.get("host.port") << "'" << endl;
        cout << "port (string): '" << config_mgr.get("port") << "'" << endl;
        
        print_separator("测试模板get方法 - 不同类型");
        
        // 测试不同类型的获取
        cout << "redis.port (int): " << config_mgr.get<int>("redis.port") << endl;
        cout << "mysql.port (string): " << config_mgr.get<string>("mysql.port") << endl;
        cout << "mysql.port (int): " << config_mgr.get<int>("mysql.port") << endl;
        cout << "other.o (bool): " << config_mgr.get<bool>("other.o") << endl;
        cout << "other.same.1 (string): " << config_mgr.get<string>("other.same.1") << endl;
        cout << "other.same.2 (string): " << config_mgr.get<string>("other.same.2") << endl;
        
        // 测试默认值
        cout << "non_existent_int (int, default=999): " << config_mgr.get<int>("non_existent_int", 999) << endl;
        cout << "non_existent_bool (bool, default=true): " << config_mgr.get<bool>("non_existent_bool", true) << endl;
        cout << "non_existent_string (string, default='default_value'): " << config_mgr.get<string>("non_existent_string", "default_value") << endl;
        
        print_separator("测试has_key方法");
        
        cout << "has_key('redis'): " << config_mgr.has_key("redis") << endl;
        cout << "has_key('redis.host'): " << config_mgr.has_key("redis.host") << endl;
        cout << "has_key('redis.port'): " << config_mgr.has_key("redis.port") << endl;
        cout << "has_key('mysql'): " << config_mgr.has_key("mysql") << endl;
        cout << "has_key('mysql.database'): " << config_mgr.has_key("mysql.database") << endl;
        cout << "has_key('other.same'): " << config_mgr.has_key("other.same") << endl;
        cout << "has_key('other.same.1'): " << config_mgr.has_key("other.same.1") << endl;
        cout << "has_key('non_existent'): " << config_mgr.has_key("non_existent") << endl;
        
        print_separator("测试新增的JSON对象获取方法");
        
        // 测试get_json_value
        json redis_json = config_mgr.get_json_value("redis");
        cout << "get_json_value('redis'): " << redis_json.dump(2) << endl;
        
        json mysql_json = config_mgr.get_json_value("mysql");
        cout << "get_json_value('mysql'): " << mysql_json.dump(2) << endl;
        
        json other_same_json = config_mgr.get_json_value("other.same");
        cout << "get_json_value('other.same'): " << other_same_json.dump(2) << endl;
        
        json host_json = config_mgr.get_json_value("host");
        cout << "get_json_value('host'): " << host_json.dump() << " (type: " << host_json.type_name() << ")" << endl;
        
        json bool_json = config_mgr.get_json_value("other.o");
        cout << "get_json_value('other.o'): " << bool_json.dump() << " (type: " << bool_json.type_name() << ")" << endl;
        
        // 测试不存在的键
        json null_json = config_mgr.get_json_value("non_existent");
        cout << "get_json_value('non_existent'): " << null_json.dump() << " (is_null: " << null_json.is_null() << ")" << endl;
        
        print_separator("测试get_json_object方法");
        
        json redis_obj = config_mgr.get_json_object("redis");
        cout << "get_json_object('redis'): " << redis_obj.dump(2) << endl;
        
        json mysql_obj = config_mgr.get_json_object("mysql");
        cout << "get_json_object('mysql'): " << mysql_obj.dump(2) << endl;
        
        json other_obj = config_mgr.get_json_object("other");
        cout << "get_json_object('other'): " << other_obj.dump(2) << endl;
        
        // 测试非对象类型（应该返回默认值）
        json host_obj = config_mgr.get_json_object("host");
        cout << "get_json_object('host') [非对象类型]: " << host_obj.dump() << endl;
        
        // 测试自定义默认值
        json custom_default = json::object({{"error", "not_found"}, {"code", 404}});
        json non_existent_obj = config_mgr.get_json_object("non_existent", custom_default);
        cout << "get_json_object('non_existent', custom_default): " << non_existent_obj.dump(2) << endl;
        
        print_separator("测试get_raw_json方法");
        
        const json& raw_json = config_mgr.get_raw_json();
        cout << "整个配置文件的JSON结构:" << endl;
        cout << raw_json.dump(2) << endl;
        
        // 直接使用raw_json进行操作
        cout << "\n直接使用raw_json访问:" << endl;
        cout << "raw_json['redis']['host']: " << raw_json["redis"]["host"] << endl;
        cout << "raw_json['mysql']['port']: " << raw_json["mysql"]["port"] << endl;
        cout << "raw_json['other']['o']: " << raw_json["other"]["o"] << endl;
        
        // 遍历other.same对象
        cout << "\n遍历other.same对象:" << endl;
        for (const auto& [key, value] : raw_json["other"]["same"].items()) {
            cout << "  " << key << " -> " << value << endl;
        }
        
        print_separator("测试get_json_string方法");
        
        // 获取整个配置的JSON字符串
        string compact_json = config_mgr.get_json_string();
        cout << "整个配置(紧凑格式): " << compact_json << endl;
        
        string formatted_json = config_mgr.get_json_string(2);
        cout << "\n整个配置(格式化):" << endl << formatted_json << endl;
        
        // 获取特定部分的JSON字符串
        string redis_str = config_mgr.get_json_string("redis");
        cout << "\nredis部分(紧凑): " << redis_str << endl;
        
        string mysql_str = config_mgr.get_json_string("mysql", 2);
        cout << "\nmysql部分(格式化):" << endl << mysql_str << endl;
        
        string other_same_str = config_mgr.get_json_string("other.same", 2);
        cout << "\nother.same部分(格式化):" << endl << other_same_str << endl;
        
        // 测试不存在的键
        string non_existent_str = config_mgr.get_json_string("non_existent");
        cout << "\n不存在键的JSON字符串: " << non_existent_str << endl;
        
        print_separator("实际应用示例");
        
        // 示例1: 数据库连接配置
        cout << "MySQL连接配置:" << endl;
        json mysql_config = config_mgr.get_json_object("mysql");
        cout << "  连接字符串: mysql://" << mysql_config["user"] 
             << ":" << mysql_config["password"] 
             << "@" << mysql_config["host"] 
             << ":" << mysql_config["port"] 
             << "/" << mysql_config["database"] << endl;
        
        // 示例2: Redis连接配置
        cout << "\nRedis连接配置:" << endl;
        string redis_host = config_mgr.get<string>("redis.host");
        int redis_port = config_mgr.get<int>("redis.port");
        cout << "  Redis地址: " << redis_host << ":" << redis_port << endl;
        
        // 示例3: 配置验证
        cout << "\n配置验证:" << endl;
        vector<string> required_keys = {"redis.host", "redis.port", "mysql.host", "mysql.user"};
        for (const string& key : required_keys) {
            if (config_mgr.has_key(key)) {
                cout << "  ✓ " << key << " 存在" << endl;
            } else {
                cout << "  ✗ " << key << " 缺失" << endl;
            }
        }
        
        print_separator("测试数组操作方法");
        
        // 测试get_array_size
        cout << "数组大小测试:" << endl;
        cout << "get_array_size('servers'): " << config_mgr.get_array_size("servers") << endl;
        cout << "get_array_size('allowed_ips'): " << config_mgr.get_array_size("allowed_ips") << endl;
        cout << "get_array_size('ports'): " << config_mgr.get_array_size("ports") << endl;
        cout << "get_array_size('tags'): " << config_mgr.get_array_size("tags") << endl;
        cout << "get_array_size('non_existent_array'): " << config_mgr.get_array_size("non_existent_array") << endl;
        
        // 测试get_array_item
        cout << "\n数组元素访问测试:" << endl;
        json first_server = config_mgr.get_array_item<json>("servers", 0);
        cout << "get_array_item('servers', 0): " << first_server.dump(2) << endl;
        
        json second_server = config_mgr.get_array_item<json>("servers", 1);  
        cout << "get_array_item('servers', 1): " << second_server.dump(2) << endl;
        
        string first_ip = config_mgr.get_array_item<string>("allowed_ips", 0);
        cout << "get_array_item('allowed_ips', 0): " << first_ip << endl;
        
        int first_port = config_mgr.get_array_item<int>("ports", 0);
        cout << "get_array_item('ports', 0): " << first_port << endl;
        
        string first_tag = config_mgr.get_array_item<string>("tags", 0);
        cout << "get_array_item('tags', 0): " << first_tag << endl;
        
        // 测试越界访问
        json out_of_bounds = config_mgr.get_array_item<json>("servers", 999);
        cout << "get_array_item('servers', 999) [越界]: " << out_of_bounds.dump() << " (is_null: " << out_of_bounds.is_null() << ")" << endl;
        
        // 测试get_array_field
        cout << "\n数组字段提取测试:" << endl;
        vector<string> server_names = config_mgr.get_array_field<string>("servers", "name");
        cout << "get_array_field('servers', 'name'): ";
        for (size_t i = 0; i < server_names.size(); ++i) {
            cout << "\"" << server_names[i] << "\"";
            if (i < server_names.size() - 1) cout << ", ";
        }
        cout << endl;
        
        vector<string> server_hosts = config_mgr.get_array_field<string>("servers", "host");
        cout << "get_array_field('servers', 'host'): ";
        for (size_t i = 0; i < server_hosts.size(); ++i) {
            cout << "\"" << server_hosts[i] << "\"";
            if (i < server_hosts.size() - 1) cout << ", ";
        }
        cout << endl;
        
        vector<int> server_ports = config_mgr.get_array_field<int>("servers", "port");
        cout << "get_array_field('servers', 'port'): ";
        for (size_t i = 0; i < server_ports.size(); ++i) {
            cout << server_ports[i];
            if (i < server_ports.size() - 1) cout << ", ";
        }
        cout << endl;
        
        vector<bool> server_enabled = config_mgr.get_array_field<bool>("servers", "enabled");
        cout << "get_array_field('servers', 'enabled'): ";
        for (size_t i = 0; i < server_enabled.size(); ++i) {
            cout << (server_enabled[i] ? "true" : "false");
            if (i < server_enabled.size() - 1) cout << ", ";
        }
        cout << endl;
        
        // 测试get_array (整个数组)
        cout << "\n整个数组获取测试:" << endl;
        vector<string> all_ips = config_mgr.get_array<string>("allowed_ips");
        cout << "get_array<string>('allowed_ips'): ";
        for (size_t i = 0; i < all_ips.size(); ++i) {
            cout << "\"" << all_ips[i] << "\"";
            if (i < all_ips.size() - 1) cout << ", ";
        }
        cout << endl;
        
        vector<int> all_ports = config_mgr.get_array<int>("ports");
        cout << "get_array<int>('ports'): ";
        for (size_t i = 0; i < all_ports.size(); ++i) {
            cout << all_ports[i];
            if (i < all_ports.size() - 1) cout << ", ";
        }
        cout << endl;
        
        vector<string> all_tags = config_mgr.get_array<string>("tags");
        cout << "get_array<string>('tags'): ";
        for (size_t i = 0; i < all_tags.size(); ++i) {
            cout << "\"" << all_tags[i] << "\"";
            if (i < all_tags.size() - 1) cout << ", ";
        }
        cout << endl;
        
        // 使用传统方式访问数组元素
        cout << "\n传统索引访问方式:" << endl;
        size_t server_count = config_mgr.get_array_size("servers");
        for (size_t i = 0; i < server_count; ++i) {
            string name = config_mgr.get<string>("servers." + to_string(i) + ".name");
            string host = config_mgr.get<string>("servers." + to_string(i) + ".host");
            int port = config_mgr.get<int>("servers." + to_string(i) + ".port");
            bool enabled = config_mgr.get<bool>("servers." + to_string(i) + ".enabled");
            
            cout << "Server " << i << ": " << name << " @ " << host << ":" << port 
                 << " (enabled: " << (enabled ? "yes" : "no") << ")" << endl;
        }
        
        // 实际应用示例：服务器配置管理
        cout << "\n实际应用示例 - 服务器配置管理:" << endl;
        cout << "启用的服务器:" << endl;
        for (size_t i = 0; i < server_count; ++i) {
            bool enabled = config_mgr.get<bool>("servers." + to_string(i) + ".enabled");
            if (enabled) {
                string name = config_mgr.get<string>("servers." + to_string(i) + ".name");
                string host = config_mgr.get<string>("servers." + to_string(i) + ".host");
                int port = config_mgr.get<int>("servers." + to_string(i) + ".port");
                cout << "  ✓ " << name << " -> http://" << host << ":" << port << endl;
            }
        }
        
        cout << "\n禁用的服务器:" << endl;
        for (size_t i = 0; i < server_count; ++i) {
            bool enabled = config_mgr.get<bool>("servers." + to_string(i) + ".enabled");
            if (!enabled) {
                string name = config_mgr.get<string>("servers." + to_string(i) + ".name");
                string host = config_mgr.get<string>("servers." + to_string(i) + ".host");
                int port = config_mgr.get<int>("servers." + to_string(i) + ".port");
                cout << "  ✗ " << name << " -> http://" << host << ":" << port << endl;
            }
        }
        
        print_separator("测试set和save方法");
        
        // 测试设置新值
        config_mgr.set("test.new_string", string("hello world"));
        config_mgr.set("test.new_int", 12345);
        config_mgr.set("test.new_bool", true);
        config_mgr.set("test.new_double", 3.14159);
        
        cout << "设置后的值:" << endl;
        cout << "test.new_string: " << config_mgr.get<string>("test.new_string") << endl;
        cout << "test.new_int: " << config_mgr.get<int>("test.new_int") << endl;
        cout << "test.new_bool: " << config_mgr.get<bool>("test.new_bool") << endl;
        cout << "test.new_double: " << config_mgr.get<double>("test.new_double") << endl;
        
        // 显示更新后的完整JSON
        cout << "\n更新后的完整配置:" << endl;
        cout << config_mgr.get_json_string(2) << endl;
        
        // 保存到文件（可选，取消注释以实际保存）
        // config_mgr.save();
        // cout << "\n配置已保存到文件" << endl;
        
        print_separator("测试完成");
        cout << "所有测试用例执行完毕！" << endl;
        
    } catch (const exception& e) {
        cerr << "发生异常: " << e.what() << endl;
       
    }

}


void test2(){
    ConfigManager config_mgr("../config.json");

    auto names = config_mgr.get_array_field<json>("config.servers", "name");
    for(auto& name : names){
        cout << name << endl;
    }

}

int main() {
    test2();
    return 0;
}
