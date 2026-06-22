#ifndef GATEWAY_HTTP_RESPONSE_BUILDER_HPP
#define GATEWAY_HTTP_RESPONSE_BUILDER_HPP

#include <nlohmann/json.hpp>

#include "../../common/utils/http_utils.hpp"
#include "http_types.hpp"

namespace im::gateway {

inline void build_http_response(HttpResponse& res,
                                int status_code,
                                const std::string& body,
                                const std::string& err_message) {
    res.status = status_code;
    res.set_content(im::utils::HttpUtils::buildResponseString(status_code, body, err_message),
                    "application/json");
}

inline void build_http_response(HttpResponse& res, const nlohmann::json& json_body) {
    if (json_body.contains("code")) {
        res.status = json_body["code"].get<int>();
    }
    res.set_content(json_body.dump(), "application/json");
}

inline void build_http_response(HttpResponse& res, const std::string& json_body) {
    res.status = im::utils::HttpUtils::statusCodeFromJson(json_body);
    res.set_content(json_body, "application/json");
}

} // namespace im::gateway

#endif // GATEWAY_HTTP_RESPONSE_BUILDER_HPP
