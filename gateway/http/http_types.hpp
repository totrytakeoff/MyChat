#ifndef GATEWAY_HTTP_TYPES_HPP
#define GATEWAY_HTTP_TYPES_HPP

#include <string>
#include <unordered_map>

namespace im::gateway {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> params;

    bool has_param(const std::string& key) const {
        return params.find(key) != params.end();
    }

    void set_header(const std::string& key, const std::string& value) {
        headers[key] = value;
    }

    std::string get_param_value(const std::string& key) const {
        auto it = params.find(key);
        return it == params.end() ? "" : it->second;
    }
};

struct HttpResponse {
    int status = 200;
    std::string body;
    std::string content_type = "application/json";

    void set_content(std::string content, std::string type) {
        body = std::move(content);
        content_type = std::move(type);
    }
};

} // namespace im::gateway

#endif // GATEWAY_HTTP_TYPES_HPP
