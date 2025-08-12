#pragma once
 
#ifndef GLOBAL_HPP
#define GLOBAL_HPP




#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <memory>
#include <chrono>
#include <mutex>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>


#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace asio = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>


// #include <grpc.h>
#include <nlohmann/json.hpp>


#include "time_utils.hpp"

static constexpr size_t HEADER_SIZE = 5; // 4 bytes for length + 1 byte for type
enum class HeaderMsgType :uint8_t{
    NORMAL,
    PING,
    PONG,
    UNKNOWN,
    ENUM_END
};

enum class PlatformType :uint8_t{
    UNKNOWN,
    WEB,
    MiniApp,
    IOS,
    ANDROID,
    DESKTOP,
    MOBILE,
    ENUM_END
};

#endif // GLOBAL_HPP

