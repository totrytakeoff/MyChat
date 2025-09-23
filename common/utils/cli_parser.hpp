#ifndef CLI_PARSER_HPP
#define CLI_PARSER_HPP

/******************************************************************************
 *
 * @file       cli_parser.hpp
 * @brief      通用命令行参数解析器 - 支持回调机制的灵活参数处理
 *             提供跨平台的命令行参数解析功能
 *
 * @details    主要功能：
 *             - 支持长短选项（--help, -h）
 *             - 回调机制处理参数
 *             - 自动生成帮助信息
 *             - 参数验证和类型转换
 *             - 支持必需和可选参数
 *             - 支持参数分组
 *
 * @author     myself
 * @date       2025/09/18
 * @version    1.0
 *
 *****************************************************************************/

#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <getopt.h>

namespace im {
namespace utils {

/**
 * @brief 参数类型枚举
 */
enum class ArgumentType {
    FLAG,           // 标志参数（无值）
    STRING,         // 字符串参数
    INTEGER,        // 整数参数
    FLOAT,          // 浮点数参数
    BOOLEAN         // 布尔参数
};

/**
 * @brief 参数回调函数类型
 * @param value 参数值（对于FLAG类型，value为"true"）
 * @return 是否处理成功
 */
using ArgumentCallback = std::function<bool(const std::string& value)>;

/**
 * @brief 参数定义结构
 */
struct ArgumentDefinition {
    std::string long_name;          // 长选项名（如 "help"）
    char short_name;                // 短选项名（如 'h'，0表示无短选项）
    ArgumentType type;              // 参数类型
    bool required;                  // 是否必需
    std::string description;        // 参数描述
    std::string default_value;      // 默认值
    std::string group;              // 参数分组（用于帮助信息分类）
    ArgumentCallback callback;      // 回调函数
    
    ArgumentDefinition() : short_name(0), type(ArgumentType::FLAG), required(false) {}
};

/**
 * @brief 解析结果结构
 */
struct ParseResult {
    bool success;                   // 解析是否成功
    std::string error_message;      // 错误信息
    std::vector<std::string> remaining_args;  // 剩余的非选项参数
    
    ParseResult() : success(true) {}
};

/**
 * @brief 通用命令行参数解析器
 * 
 * @details 提供灵活的命令行参数解析功能：
 *          - 支持回调机制处理参数
 *          - 自动类型转换和验证
 *          - 自动生成帮助信息
 *          - 支持参数分组显示
 */
class CLIParser {
public:
    /**
     * @brief 构造函数
     * @param program_name 程序名称
     * @param program_description 程序描述
     */
    explicit CLIParser(const std::string& program_name, 
                      const std::string& program_description = "")
        : program_name_(program_name), program_description_(program_description) {
        
        // 添加默认的帮助和版本参数
        addArgument("help", 'h', ArgumentType::FLAG, false, "Show this help message", "", "General",
                   [this](const std::string&) -> bool {
                       printHelp();
                       return true;
                   });
    }

    /**
     * @brief 添加参数定义
     * @param long_name 长选项名
     * @param short_name 短选项名（0表示无短选项）
     * @param type 参数类型
     * @param required 是否必需
     * @param description 参数描述
     * @param default_value 默认值
     * @param group 参数分组
     * @param callback 回调函数
     * @return 是否添加成功
     */
    bool addArgument(const std::string& long_name, char short_name, ArgumentType type,
                    bool required, const std::string& description, 
                    const std::string& default_value = "", const std::string& group = "General",
                    ArgumentCallback callback = nullptr) {
        
        if (long_name.empty()) {
            return false;
        }
        
        // 检查重复
        if (arguments_.find(long_name) != arguments_.end()) {
            return false;
        }
        
        if (short_name != 0 && short_to_long_.find(short_name) != short_to_long_.end()) {
            return false;
        }
        
        ArgumentDefinition def;
        def.long_name = long_name;
        def.short_name = short_name;
        def.type = type;
        def.required = required;
        def.description = description;
        def.default_value = default_value;
        def.group = group;
        def.callback = callback;
        
        arguments_[long_name] = def;
        
        if (short_name != 0) {
            short_to_long_[short_name] = long_name;
        }
        
        return true;
    }

    /**
     * @brief 添加版本参数
     * @param version 版本字符串
     * @param callback 自定义版本显示回调（可选）
     */
    void addVersionArgument(const std::string& version, ArgumentCallback callback = nullptr) {
        version_ = version;
        
        auto version_callback = callback ? callback : 
            [this](const std::string&) -> bool {
                printVersion();
                return true;
            };
            
        addArgument("version", 'v', ArgumentType::FLAG, false, "Show version information", "", "General",
                   version_callback);
    }

    /**
     * @brief 解析命令行参数
     * @param argc 参数个数
     * @param argv 参数数组
     * @return 解析结果
     */
    ParseResult parse(int argc, char* argv[]) {
        ParseResult result;
        
        try {
            // 构建getopt_long所需的结构
            std::vector<struct option> long_options;
            std::string short_options;
            
            buildGetoptStructures(long_options, short_options);
            
            // 解析参数
            int option_index = 0;
            int c;
            
            // 重置getopt状态
            optind = 1;
            
            while ((c = getopt_long(argc, argv, short_options.c_str(), 
                                   long_options.data(), &option_index)) != -1) {
                
                if (!processOption(c, optarg, result)) {
                    return result;
                }
            }
            
            // 收集剩余参数
            for (int i = optind; i < argc; i++) {
                result.remaining_args.push_back(argv[i]);
            }
            
            // 检查必需参数
            if (!checkRequiredArguments(result)) {
                return result;
            }
            
            // 处理默认值
            processDefaultValues();
            
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = std::string("Parse error: ") + e.what();
        }
        
        return result;
    }

    /**
     * @brief 打印帮助信息
     */
    void printHelp() const {
        std::cout << program_name_;
        if (!program_description_.empty()) {
            std::cout << " - " << program_description_;
        }
        std::cout << "\n\n";
        
        std::cout << "USAGE:\n";
        std::cout << "    " << program_name_ << " [OPTIONS]";
        
        // 显示位置参数（如果有）
        std::cout << "\n\n";
        
        // 按组显示选项
        std::map<std::string, std::vector<const ArgumentDefinition*>> groups;
        for (const auto& [name, def] : arguments_) {
            groups[def.group].push_back(&def);
        }
        
        for (const auto& [group_name, group_args] : groups) {
            std::cout << group_name << " OPTIONS:\n";
            
            for (const auto* def : group_args) {
                std::cout << "    ";
                
                // 短选项
                if (def->short_name != 0) {
                    std::cout << "-" << def->short_name << ", ";
                } else {
                    std::cout << "    ";
                }
                
                // 长选项
                std::cout << "--" << def->long_name;
                
                // 参数类型提示
                if (def->type != ArgumentType::FLAG) {
                    std::cout << " <";
                    switch (def->type) {
                        case ArgumentType::STRING:  std::cout << "STRING"; break;
                        case ArgumentType::INTEGER: std::cout << "INT"; break;
                        case ArgumentType::FLOAT:   std::cout << "FLOAT"; break;
                        case ArgumentType::BOOLEAN: std::cout << "BOOL"; break;
                        default: break;
                    }
                    std::cout << ">";
                }
                
                // 对齐描述
                std::cout << std::string(std::max(1, 25 - static_cast<int>(def->long_name.length())), ' ');
                std::cout << def->description;
                
                // 显示默认值
                if (!def->default_value.empty()) {
                    std::cout << " (default: " << def->default_value << ")";
                }
                
                // 显示必需标记
                if (def->required) {
                    std::cout << " [REQUIRED]";
                }
                
                std::cout << "\n";
            }
            std::cout << "\n";
        }
    }

    /**
     * @brief 打印版本信息
     */
    void printVersion() const {
        std::cout << program_name_;
        if (!version_.empty()) {
            std::cout << " version " << version_;
        }
        std::cout << std::endl;
    }

private:
    /**
     * @brief 构建getopt_long所需的数据结构
     */
    void buildGetoptStructures(std::vector<struct option>& long_options, 
                              std::string& short_options) {
        
        for (const auto& [name, def] : arguments_) {
            // 长选项
            struct option opt;
            opt.name = def.long_name.c_str();
            opt.has_arg = (def.type == ArgumentType::FLAG) ? no_argument : required_argument;
            opt.flag = nullptr;
            opt.val = def.short_name != 0 ? def.short_name : (1000 + long_options.size());
            long_options.push_back(opt);
            
            // 短选项
            if (def.short_name != 0) {
                short_options += def.short_name;
                if (def.type != ArgumentType::FLAG) {
                    short_options += ':';
                }
            }
        }
        
        // 结束标记
        struct option end_opt = {nullptr, 0, nullptr, 0};
        long_options.push_back(end_opt);
    }

    /**
     * @brief 处理单个选项
     */
    bool processOption(int option_char, char* optarg_value, ParseResult& result) {
        std::string long_name;
        
        // 查找对应的长选项名
        if (option_char >= 1000) {
            // 通过索引查找
            int index = option_char - 1000;
            auto it = arguments_.begin();
            std::advance(it, index);
            long_name = it->first;
        } else {
            auto it = short_to_long_.find(option_char);
            if (it != short_to_long_.end()) {
                long_name = it->second;
            } else {
                result.success = false;
                result.error_message = std::string("Unknown option: -") + static_cast<char>(option_char);
                return false;
            }
        }
        
        auto arg_it = arguments_.find(long_name);
        if (arg_it == arguments_.end()) {
            result.success = false;
            result.error_message = "Internal error: option not found";
            return false;
        }
        
        const auto& def = arg_it->second;
        std::string value;
        
        if (def.type == ArgumentType::FLAG) {
            value = "true";
        } else if (optarg_value) {
            value = optarg_value;
        } else {
            result.success = false;
            result.error_message = "Option --" + long_name + " requires an argument";
            return false;
        }
        
        // 类型验证
        if (!validateArgumentValue(def, value, result)) {
            return false;
        }
        
        // 执行回调
        if (def.callback) {
            try {
                if (!def.callback(value)) {
                    result.success = false;
                    result.error_message = "Callback failed for option --" + long_name;
                    return false;
                }
            } catch (const std::exception& e) {
                result.success = false;
                result.error_message = std::string("Callback error for --") + long_name + ": " + e.what();
                return false;
            }
        }
        
        parsed_arguments_[long_name] = value;
        return true;
    }

    /**
     * @brief 验证参数值
     */
    bool validateArgumentValue(const ArgumentDefinition& def, const std::string& value, 
                              ParseResult& result) {
        
        try {
            switch (def.type) {
                case ArgumentType::INTEGER:
                    std::stoi(value);
                    break;
                case ArgumentType::FLOAT:
                    std::stof(value);
                    break;
                case ArgumentType::BOOLEAN:
                    if (value != "true" && value != "false" && value != "1" && value != "0" &&
                        value != "yes" && value != "no" && value != "on" && value != "off") {
                        throw std::invalid_argument("Invalid boolean value");
                    }
                    break;
                default:
                    break;
            }
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = "Invalid value '" + value + "' for option --" + def.long_name + 
                                  ": " + e.what();
            return false;
        }
        
        return true;
    }

    /**
     * @brief 检查必需参数
     */
    bool checkRequiredArguments(ParseResult& result) {
        for (const auto& [name, def] : arguments_) {
            if (def.required && parsed_arguments_.find(name) == parsed_arguments_.end()) {
                result.success = false;
                result.error_message = "Required option --" + name + " is missing";
                return false;
            }
        }
        return true;
    }

    /**
     * @brief 处理默认值
     */
    void processDefaultValues() {
        for (const auto& [name, def] : arguments_) {
            if (parsed_arguments_.find(name) == parsed_arguments_.end() && 
                !def.default_value.empty() && def.callback) {
                
                try {
                    def.callback(def.default_value);
                    parsed_arguments_[name] = def.default_value;
                } catch (...) {
                    // 忽略默认值处理错误
                }
            }
        }
    }

private:
    std::string program_name_;
    std::string program_description_;
    std::string version_;
    
    std::map<std::string, ArgumentDefinition> arguments_;
    std::map<char, std::string> short_to_long_;
    std::map<std::string, std::string> parsed_arguments_;
};

}  // namespace utils
}  // namespace im

#endif  // CLI_PARSER_HPP