#ifndef GATEWAY_HTTPLIB_ADAPTER_HPP
#define GATEWAY_HTTPLIB_ADAPTER_HPP

#include <httplib.h>

#include "http_types.hpp"

namespace im::gateway {

inline HttpRequest from_httplib_request(const httplib::Request& req) {
    HttpRequest out;
    out.method = req.method;
    out.path = req.path;
    out.body = req.body;
    for (const auto& [key, value] : req.headers) {
        out.headers.emplace(key, value);
    }
    for (const auto& [key, value] : req.params) {
        out.params.emplace(key, value);
    }
    return out;
}

inline void apply_http_response(const HttpResponse& from, httplib::Response& to) {
    to.status = from.status;
    to.set_content(from.body, from.content_type);
}

} // namespace im::gateway

#endif // GATEWAY_HTTPLIB_ADAPTER_HPP
