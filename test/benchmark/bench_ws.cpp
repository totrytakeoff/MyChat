#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <numeric>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <fstream>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <nlohmann/json.hpp>

#include <google/protobuf/message.h>

#include "common/network/protobuf_codec.hpp"
#include "common/proto/base.pb.h"
#include "common/proto/command.pb.h"
#include "common/proto/message.pb.h"
#include "common/utils/log_manager.hpp"

namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace http = beast::http;
using tcp = net::ip::tcp;
using json = nlohmann::json;
using namespace std::chrono;

// --------------- Config ---------------

struct UserToken {
    std::string uid;
    std::string account;
    std::string access_token;
    std::string device_id;
    std::string platform;
};

struct BenchConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 10001;
    std::string token_file = "tokens.json";
    int max_users = 0;
    int duration_sec = 30;
    int message_interval_ms = 1000;
    int conn_timeout_ms = 10000;
    std::string scenario = "ping";
};

// --------------- Stats ---------------

struct alignas(64) SampleSink {
    static constexpr size_t MAX = 2 << 20;
    double* samples;
    std::atomic<size_t> count{0};
    std::atomic<size_t> dropped{0};

    SampleSink() {
        samples = new double[MAX];
    }
    ~SampleSink() { delete[] samples; }

    void push(double v) {
        size_t pos = count.fetch_add(1, std::memory_order_relaxed);
        if (pos < MAX)
            samples[pos] = v;
        else
            dropped.fetch_add(1, std::memory_order_relaxed);
    }

    size_t size() const { return std::min(count.load(), MAX); }
};

struct BenchStats {
    std::atomic<int64_t> connects_ok{0};
    std::atomic<int64_t> connects_fail{0};
    std::atomic<int64_t> msgs_sent{0};
    std::atomic<int64_t> msgs_recv{0};
    std::atomic<int64_t> errors{0};
    std::atomic<int64_t> bytes_sent{0};
    std::atomic<int64_t> bytes_recv{0};

    SampleSink rtt_ms;
    SampleSink connect_time_ms;
};

static BenchStats g_stats;

// --------------- Session ---------------

class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    WsSession(net::io_context& ioc, ssl::context& ssl_ctx)
        : resolver_(ioc)
        , ws_(ioc, ssl_ctx)
    {
    }

    ~WsSession() { close(); }

    void set_token(const UserToken& t) {
        token_ = t;
        uri_ = "/?token=" + t.access_token;
    }

    void set_receiver_uid(const std::string& uid) { receiver_uid_ = uid; }

    void start() {
        auto self = shared_from_this();
        connect_start_ = high_resolution_clock::now();

        resolver_.async_resolve(
            host_, std::to_string(port_),
            [self](beast::error_code ec, tcp::resolver::results_type results) {
                if (ec) { self->fail("resolve", ec); return; }
                self->on_resolve(results);
            });
    }

    void set_host(const std::string& h) { host_ = h; }
    void set_port(uint16_t p) { port_ = p; }
    void set_interval_ms(int ms) { interval_ms_ = ms; }

    void stop() { close(); }

    bool connected() const { return connected_; }

private:
    void on_resolve(tcp::resolver::results_type results) {
        auto self = shared_from_this();
        auto ep = results.begin()->endpoint();
        beast::get_lowest_layer(ws_).async_connect(
            ep,
            [self](beast::error_code ec) {
                if (ec) { self->fail("connect", ec); return; }
                self->on_tcp_connect();
            });
    }

    void on_tcp_connect() {
        auto self = shared_from_this();
        ws_.next_layer().async_handshake(
            ssl::stream_base::client,
            [self](beast::error_code ec) {
                if (ec) { self->fail("ssl", ec); return; }
                self->on_ssl_handshake();
            });
    }

    void on_ssl_handshake() {
        auto self = shared_from_this();
        ws_.set_option(websocket::stream_base::decorator(
            [](auto& req) {
                req.set(http::field::user_agent, "MyChat-Bench/1.0");
            }));
        ws_.async_handshake(host_, uri_,
            [self](beast::error_code ec) {
                if (ec) { self->fail("ws_handshake", ec); return; }
                self->on_ws_handshake();
            });
    }

    void on_ws_handshake() {
        ws_.binary(true);
        connected_ = true;
        auto ms = duration_cast<milliseconds>(high_resolution_clock::now() - connect_start_).count();
        g_stats.connect_time_ms.push(static_cast<double>(ms));
        g_stats.connects_ok.fetch_add(1);

        do_read();
        schedule_send();
    }

    void schedule_send() {
        if (!connected_) return;
        auto self = shared_from_this();
        timer_.expires_after(milliseconds(interval_ms_));
        timer_.async_wait([self](beast::error_code ec) {
            if (ec) return;
            self->do_send();
            self->schedule_send();
        });
    }

    void do_send() {
        if (!connected_) return;
        auto self = shared_from_this();

        im::base::IMHeader hdr;
        uint32_t seq = seq_.fetch_add(1);
        hdr.set_version("1.0");
        hdr.set_seq(seq);
        hdr.set_cmd_id(im::command::CMD_SEND_MESSAGE);
        hdr.set_from_uid(token_.uid);
        hdr.set_to_uid(receiver_uid_);
        auto ts = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        hdr.set_timestamp(ts);
        hdr.set_token(token_.access_token);
        hdr.set_device_id(token_.device_id);
        hdr.set_platform(token_.platform);

        im::message::SendMessageRequest send_req;
        *send_req.mutable_header() = hdr;

        auto* body = send_req.mutable_body();
        body->set_message_id(std::to_string(seq));
        body->set_type(im::message::TEXT);
        body->set_content("bench");
        body->set_sender_uid(token_.uid);
        body->set_receiver_uid(receiver_uid_);
        body->set_create_time(ts);

        std::string frame;
        if (!im::network::ProtobufCodec::encode(hdr, send_req, frame)) {
            g_stats.errors.fetch_add(1);
            return;
        }

        pending_seqs_[seq] = high_resolution_clock::now();

        g_stats.msgs_sent.fetch_add(1);
        g_stats.bytes_sent.fetch_add(frame.size());

        ws_.async_write(net::buffer(frame),
            [self, seq](beast::error_code ec, size_t) {
                if (ec) self->fail("write", ec);
            });
    }

    void do_read() {
        if (!connected_) return;
        auto self = shared_from_this();
        buffer_.clear();
        ws_.async_read(buffer_,
            [self](beast::error_code ec, size_t) {
                if (ec == websocket::error::closed) {
                    self->close();
                    return;
                }
                if (ec) { self->fail("read", ec); return; }
                self->on_read();
            });
    }

    void on_read() {
        g_stats.msgs_recv.fetch_add(1);
        g_stats.bytes_recv.fetch_add(buffer_.size());

        std::string data = beast::buffers_to_string(buffer_.data());
        auto now = high_resolution_clock::now();

        im::base::IMHeader resp_hdr;
        std::string type_name, body_bytes;
        if (im::network::ProtobufCodec::decodeEnvelope(data, resp_hdr, type_name, body_bytes)) {
            uint32_t seq = resp_hdr.seq();
            auto it = pending_seqs_.find(seq);
            if (it != pending_seqs_.end()) {
                double ms = duration_cast<microseconds>(now - it->second).count() / 1000.0;
                g_stats.rtt_ms.push(ms);
                pending_seqs_.erase(it);
            }
        } else {
            g_stats.errors.fetch_add(1);
        }

        do_read();
    }

    void fail(const char* op, beast::error_code ec) {
        if (ec == net::error::operation_aborted) return;
        if (!connected_) {
            auto ms = duration_cast<milliseconds>(high_resolution_clock::now() - connect_start_).count();
            g_stats.connect_time_ms.push(static_cast<double>(ms));
            g_stats.connects_fail.fetch_add(1);
        }
        g_stats.errors.fetch_add(1);
        close();
    }

    void close() {
        if (!connected_) return;
        connected_ = false;
        timer_.cancel();
        try {
            ws_.close(websocket::close_code::normal);
        } catch (...) {}
    }

    std::string host_;
    uint16_t port_ = 10001;
    std::string uri_;
    UserToken token_;
    int interval_ms_ = 1000;

    tcp::resolver resolver_;
    websocket::stream<beast::ssl_stream<tcp::socket>> ws_;
    beast::flat_buffer buffer_;
    net::steady_timer timer_{ws_.get_executor()};

    std::atomic<uint32_t> seq_{1};
    std::map<uint32_t, high_resolution_clock::time_point> pending_seqs_;
    std::mutex pending_mutex_;

    high_resolution_clock::time_point connect_start_;
    std::atomic<bool> connected_{false};
    std::string receiver_uid_;
};

// --------------- Helpers ---------------

static std::vector<UserToken> load_tokens(const std::string& path) {
    std::vector<UserToken> tokens;
    std::ifstream f(path);
    if (!f) {
        std::cerr << "Cannot open " << path << std::endl;
        return tokens;
    }
    json j;
    f >> j;
    for (auto& item : j) {
        UserToken t;
        t.uid = item.value("uid", "");
        t.account = item.value("account", "");
        t.access_token = item.value("access_token", "");
        t.device_id = item.value("device_id", "bench-device");
        t.platform = item.value("platform", "bench");
        tokens.push_back(t);
    }
    return tokens;
}

static void print_stats() {
    auto& rtt = g_stats.rtt_ms;
    auto& ctm = g_stats.connect_time_ms;

    auto fmt = [](const std::string& label, const SampleSink& s) {
        size_t n = s.size();
        if (n == 0) {
            std::cout << "  " << label << ": no samples" << std::endl;
            return;
        }

        std::vector<double> v(s.samples, s.samples + n);
        std::sort(v.begin(), v.end());

        double sum = std::accumulate(v.begin(), v.end(), 0.0);
        double avg = sum / n;
        double min = v.front();
        double max = v.back();
        double p50 = v[static_cast<size_t>(n * 0.50)];
        double p90 = v[static_cast<size_t>(n * 0.90)];
        double p95 = v[static_cast<size_t>(n * 0.95)];
        double p99 = v[static_cast<size_t>(n * 0.99)];

        std::cout << "  " << label << ":" << std::endl;
        std::cout << "    count: " << n << std::endl;
        if (s.dropped.load())
            std::cout << "    dropped: " << s.dropped.load() << std::endl;
        std::cout << "    min/avg/max: " << std::fixed << std::setprecision(2)
                  << min << " / " << avg << " / " << max << " ms" << std::endl;
        std::cout << "    p50/p90/p95/p99: " << p50 << " / " << p90 << " / "
                  << p95 << " / " << p99 << " ms" << std::endl;
    };

    std::cout << "\n======= Results =======" << std::endl;
    std::cout << "Connects OK:   " << g_stats.connects_ok.load() << std::endl;
    std::cout << "Connects FAIL: " << g_stats.connects_fail.load() << std::endl;
    std::cout << "Messages sent: " << g_stats.msgs_sent.load() << std::endl;
    std::cout << "Messages recv: " << g_stats.msgs_recv.load() << std::endl;
    std::cout << "Errors:        " << g_stats.errors.load() << std::endl;
    std::cout << "Bytes sent:    " << g_stats.bytes_sent.load() << std::endl;
    std::cout << "Bytes recv:    " << g_stats.bytes_recv.load() << std::endl;

    fmt("Connect time", ctm);
    fmt("RTT", rtt);
}

// --------------- main ---------------

int main(int argc, char* argv[]) {
    BenchConfig cfg;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) cfg.host = argv[++i];
        else if (arg == "--port" && i + 1 < argc) cfg.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        else if (arg == "--tokens" && i + 1 < argc) cfg.token_file = argv[++i];
        else if (arg == "--users" && i + 1 < argc) cfg.max_users = std::stoi(argv[++i]);
        else if (arg == "--duration" && i + 1 < argc) cfg.duration_sec = std::stoi(argv[++i]);
        else if (arg == "--interval" && i + 1 < argc) cfg.message_interval_ms = std::stoi(argv[++i]);
        else if (arg == "--scenario" && i + 1 < argc) cfg.scenario = argv[++i];
        else if (arg == "--timeout" && i + 1 < argc) cfg.conn_timeout_ms = std::stoi(argv[++i]);
        else if (arg == "--help") {
            std::cout << "Usage: bench_ws [options]\n"
                      << "  --host HOST          Server host (default: 127.0.0.1)\n"
                      << "  --port PORT          Server port (default: 10001)\n"
                      << "  --tokens FILE        Token JSON file (default: tokens.json)\n"
                      << "  --users N            Max users to connect (0 = all)\n"
                      << "  --duration SEC       Test duration (default: 30)\n"
                      << "  --interval MS        Message interval per user (default: 1000)\n"
                      << "  --scenario NAME      Scenario: ping (default)\n"
                      << "  --timeout MS         Connection timeout (default: 10000)\n";
            return 0;
        }
    }

    im::utils::LogManager::SetLoggingEnabled("protobuf_codec", false);

    auto tokens = load_tokens(cfg.token_file);
    if (tokens.empty()) {
        std::cerr << "No tokens loaded. Run prep_users.py first." << std::endl;
        return 1;
    }
    if (cfg.max_users > 0 && static_cast<size_t>(cfg.max_users) < tokens.size())
        tokens.resize(cfg.max_users);

    std::cout << "Loaded " << tokens.size() << " users" << std::endl;
    std::cout << "Server: " << cfg.host << ":" << cfg.port << std::endl;
    std::cout << "Duration: " << cfg.duration_sec << "s" << std::endl;
    std::cout << "Interval: " << cfg.message_interval_ms << "ms/user" << std::endl;
    std::cout << "Scenario: " << cfg.scenario << std::endl;

    net::io_context ioc;
    ssl::context ssl_ctx(ssl::context::tlsv12_client);
    ssl_ctx.set_default_verify_paths();
    ssl_ctx.set_verify_mode(ssl::verify_none);

    std::vector<std::shared_ptr<WsSession>> sessions;
    for (size_t i = 0; i < tokens.size(); ++i) {
        auto& t = tokens[i];
        auto& next_t = tokens[(i + 1) % tokens.size()];
        auto s = std::make_shared<WsSession>(ioc, ssl_ctx);
        s->set_host(cfg.host);
        s->set_port(cfg.port);
        s->set_token(t);
        s->set_receiver_uid(next_t.uid);
        s->set_interval_ms(cfg.message_interval_ms);
        sessions.push_back(s);
    }

    for (auto& s : sessions)
        s->start();

    std::thread worker([&] { ioc.run(); });

    std::this_thread::sleep_for(seconds(cfg.duration_sec));

    for (auto& s : sessions)
        s->stop();

    ioc.stop();
    if (worker.joinable())
        worker.join();

    print_stats();

    return g_stats.connects_fail.load() > 0 ? 1 : 0;
}
