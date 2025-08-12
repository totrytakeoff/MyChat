#include "../../common/utils/config_mgr.hpp"
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>

using namespace std;
using namespace im::utils;
using json = nlohmann::json;

int main (){

    ConfigManager config_mgr("config.json");

    cout<< config_mgr.get("redis.host")<<endl;
    cout<< config_mgr.get<int>("redis.port")<<endl;

    cout<< config_mgr.get("mysql.host")<<endl;
    cout<< config_mgr.get("mysql.user")<<endl;

    cout<< config_mgr.get("other.same.1")<<endl;
    cout<< config_mgr.get<bool>("other.o")<<endl;
    cout<< config_mgr.get("host")<<endl;

    cout<< config_mgr.get("host.port")<<endl;
    cout<< config_mgr.get("port")<<endl;
    
// error method
    // cout<<"-----"<<endl;

    // auto j = config_mgr.get("redis");
    // cout<< "redis" << j<<endl;
    // // cout<< "json" << j.dump()<<endl;
    // // cout<< "json" << j["host"].get<std::string>()<<endl;

    // cout<<"-----"<<endl;


    // string m  = config_mgr.get("mysql");
    // auto mysql = json::parse(m);
    // cout<< "mysql" << m<<endl;
    // cout<< "mysql" << mysql["host"].get<std::string>()<<endl;




    return 0;
}
