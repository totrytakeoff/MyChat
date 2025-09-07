#ifndef HTTP_UTILS_HPP
#define HTTP_UTILS_HPP

/******************************************************************************
 *
 * @file       http_utils.hpp
 * @brief      基于httplib的HTTP辅助工具类
 *             提供响应构建和状态码解析功能
 *
 * @author     myself
 * @date       2025/09/07
 *
 *****************************************************************************/

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

class HttpUtils {
public:
    enum StatusLevel { ERROR, WARNING, INFO };

public:
    /**
     * @brief 构建JSON格式的响应字符串
     * @param status_code HTTP状态码
     * @param body 响应体内容
     * @param err_message 错误信息
     * @return 格式化的JSON响应字符串
     */
    static std::string buildResponseString(int status_code, const std::string& body,
                                           const std::string& err_message) {
        json response;
        response["code"] = status_code;
        response["body"] = body;
        response["error_message"] = err_message;
        return response.dump();
    }

    /**
     * @brief 构建统一格式的JSON响应字符串
     * @param code 状态码
     * @param body JSON格式的响应体内容，可以为null
     * @param err_msg 错误信息
     * @return 统一格式的JSON响应字符串: {"code": xxx, "body": xxx, "err_msg": xxx}
     */
    static std::string buildUnifiedResponse(int code, const json& body = nullptr, 
                                          const std::string& err_msg = "") {
        json response;
        response["code"] = code;
        response["body"] = body.is_null() ? nullptr : body;
        response["err_msg"] = err_msg;
        return response.dump();
    }

    /**
     * @brief 直接构建httplib响应对象
     * @param res httplib响应对象引用
     * @param status_code HTTP状态码
     * @param body 响应体内容
     * @param err_message 错误信息
     */
    static void buildResponse(httplib::Response& res, int status_code, const std::string& body,
                              const std::string& err_message) {
        res.status = status_code;
        res.set_content(buildResponseString(status_code, body, err_message), "application/json");
    }

    /**
     * @brief 使用JSON对象构建响应
     * @param res httplib响应对象引用
     * @param json_body JSON格式的响应体
     */
    static void buildResponse(httplib::Response& res, const json& json_body) {
        if (json_body["code"]) {
            res.status = json_body["code"].get<int>();
        }
        res.set_content(json_body.dump(), "application/json");
    }


    static int statusCodeFromJson(const json& json_body) {
        if (json_body["code"]) {
            return json_body["code"].get<int>();
        }

        return 500;
    }

    static int statusCodeFromJson(const std::string& json_body) {
        return statusCodeFromJson(json::parse(json_body));
    }

    static std::string err_msgFromJson(const json& json_body) {
        if (json_body["err_msg"]) {
            return json_body["err_msg"].get<std::string>();
        }
        return "";
    }
    static std::string err_msgFromJson(const std::string& json_body) {
        return err_msgFromJson(json::parse(json_body));
    }

    /**
     * @brief 使用字符串构建响应
     * @param res httplib响应对象引用
     * @param json_body JSON格式的字符串响应体
     */
    static void buildResponse(httplib::Response& res, const std::string& json_body) {
        res.status = statusCodeFromJson(json_body);
        res.set_content(json_body, "application/json");
    }

    /**
     * @brief 根据HTTP状态码判断日志等级
     * @param status_code HTTP状态码
     * @return 对应的日志等级
     */
    static StatusLevel parseStatusCode(int status_code) {
        if (status_code >= 200 && status_code < 300) {
            return INFO;
        } else if (status_code >= 400 && status_code < 500) {
            return WARNING;
        } else {
            return ERROR;
        }
    }

    /**
     * @brief 判断HTTP状态码是否表示成功
     * @param status_code HTTP状态码
     * @return 是否成功
     */
    static bool isSuccessStatus(int status_code) { return status_code >= 200 && status_code < 300; }
};

#endif  // HTTP_UTILS_HPP