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
#include <thread>
#include <utility>
#include <vector>
#include <fstream>
#include <unordered_map>

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
    int duration_sec = 60;
    int message_interval_ms = 5000;
    int conn_timeout_ms = 15000;
    int connect_rate = 0;
    int threads = 4;
    bool csv_mode = false;
};

struct SampleSink {
    static constexpr size_t MAX = 2 << 20;
    double* samples;
    std::atomic<size_t> count{0};
    std::atomic<size_t> dropped{0};

    SampleSink() { samples = new double[MAX]; }
    ~SampleSink() { delete[] samples; }

    void push(double v) {
        size_t pos = count.fetch_add(1, std::memory_order_relaxed);
        if (pos < MAX) samples[pos] = v;
        else dropped.fetch_add(1, std::memory_order_relaxed);
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
static std::atomic<int64_t> g_active_sessions{0};
static std::atomic<bool> g_stop{false};

class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    WsSession(net::io_context& ioc, ssl::context& ssl_ctx, int conn_timeout_ms)
        : resolver_(ioc)
        , ws_(ioc, ssl_ctx)
        , timer_(ioc)
        , conn_timeout_ms_(conn_timeout_ms)
    {}

    ~WsSession() { close(); }

    void set_token(const UserToken& t) {
        token_ = t;
        uri_ = "/?token=" + t.access_token;
    }
    void set_receiver_uid(const std::string& uid) { receiver_uid_ = uid; }
    void set_host(const std::string& h) { host_ = h; }
    void set_port(uint16_t p) { port_ = p; }
    void set_interval_ms(int ms) { interval_ms_ = ms; }
    void stop() { close(); }
    bool connected() const { return connected_.load(); }

    void start() {
        auto self = shared_from_this();
        connect_start_ = high_resolution_clock::now();

        conn_timer_.expires_after(milliseconds(conn_timeout_ms_));
        conn_timer_.async_wait([self](beast::error_code ec) {
            if (!ec && !self->connected_.load()) {
                self->fail("timeout", beast::error_code{});
            }
        });

        resolver_.async_resolve(
            host_, std::to_string(port_),
            [self](beast::error_code ec, tcp::resolver::results_type results) {
                if (ec) { self->fail("resolve", ec); return; }
                self->on_resolve(results);
            });
    }

private:
    void on_resolve(tcp::resolver::results_type results) {
        auto self = shared_from_this();
        beast::get_lowest_layer(ws_).async_connect(
            results.begin()->endpoint(),
            [self](beast::error_code ec) {
                if (ec) { self->fail("tcp", ec); return; }
                self->ws_.next_layer().async_handshake(
                    ssl::stream_base::client,
                    [self](beast::error_code ec) {
                        if (ec) { self->fail("ssl", ec); return; }
                        self->on_ssl_handshake();
                    });
            });
    }

    void on_ssl_handshake() {
        auto self = shared_from_this();
        ws_.set_option(websocket::stream_base::decorator(
            [](auto& req) { req.set(http::field::user_agent, "MyChat-Bench/2.0"); }));
        ws_.async_handshake(host_, uri_,
            [self](beast::error_code ec) {
                if (ec) { self->fail("ws", ec); return; }
                self->on_ws_handshake();
            });
    }

    void on_ws_handshake() {
        ws_.binary(true);
        conn_timer_.cancel();
        connected_.store(true);
        auto ms = duration_cast<milliseconds>(high_resolution_clock::now() - connect_start_).count();
        g_stats.connect_time_ms.push(static_cast<double>(ms));
        g_stats.connects_ok.fetch_add(1);
        g_active_sessions.fetch_add(1);

        do_read();
        schedule_send();
    }

    void schedule_send() {
        if (g_stop.load() || !connected_.load()) return;
        auto self = shared_from_this();
        timer_.expires_after(milliseconds(interval_ms_));
        timer_.async_wait([self](beast::error_code ec) {
            if (ec || g_stop.load()) return;
            self->do_send();
            self->schedule_send();
        });
    }

    void do_send() {
        if (!connected_.load()) return;
        auto self = shared_from_this();

        im::base::IMHeader hdr;
        uint32_t seq = seq_.fetch_add(1);
        hdr.set_version("1.0");
        hdr.set_seq(seq);
        hdr.set_cmd_id(im::command::CMD_SEND_MESSAGE);
        hdr.set_from_uid(token_.uid);
        hdr.set_to_uid(receiver_uid_);
        hdr.set_token(token_.access_token);
        hdr.set_device_id(token_.device_id);
        hdr.set_platform(token_.platform);
        auto ts = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        hdr.set_timestamp(ts);

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

        {
            std::lock_guard<std::mutex> lk(pending_mutex_);
            pending_seqs_[seq] = high_resolution_clock::now();
        }

        g_stats.msgs_sent.fetch_add(1);
        g_stats.bytes_sent.fetch_add(frame.size());

        ws_.async_write(net::buffer(frame),
            [self, seq](beast::error_code ec, size_t) {
                if (ec) { self->fail("write", ec); }
            });
    }

    void do_read() {
        if (!connected_.load()) return;
        auto self = shared_from_this();
        buffer_.clear();
        ws_.async_read(buffer_,
            [self](beast::error_code ec, size_t) {
                if (ec == websocket::error::closed || ec == net::error::operation_aborted) {
                    self->close(); return;
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
            high_resolution_clock::time_point sent_ts;
            bool found = false;
            {
                std::lock_guard<std::mutex> lk(pending_mutex_);
                auto it = pending_seqs_.find(seq);
                if (it != pending_seqs_.end()) {
                    sent_ts = it->second;
                    pending_seqs_.erase(it);
                    found = true;
                }
            }
            if (found) {
                double ms = duration_cast<microseconds>(now - sent_ts).count() / 1000.0;
                g_stats.rtt_ms.push(ms);
            }
        } else {
            g_stats.errors.fetch_add(1);
        }

        do_read();
    }

    void fail(const char* op, beast::error_code ec) {
        if (!connected_.load()) {
            auto ms = duration_cast<milliseconds>(high_resolution_clock::now() - connect_start_).count();
            g_stats.connect_time_ms.push(static_cast<double>(ms));
            g_stats.connects_fail.fetch_add(1);
        } else {
            g_stats.errors.fetch_add(1);
        }
        close();
    }

    void close() {
        bool was = connected_.exchange(false);
        if (!was) return;
        g_active_sessions.fetch_sub(1);
        conn_timer_.cancel();
        timer_.cancel();
        try { ws_.close(websocket::close_code::normal); } catch (...) {}
    }

    std::string host_;
    uint16_t port_ = 10001;
    std::string uri_;
    UserToken token_;
    int interval_ms_ = 5000;
    int conn_timeout_ms_ = 15000;

    tcp::resolver resolver_;
    websocket::stream<beast::ssl_stream<tcp::socket>> ws_;
    beast::flat_buffer buffer_;
    net::steady_timer timer_;
    net::steady_timer conn_timer_{timer_.get_executor()};

    std::atomic<uint32_t> seq_{1};
    std::unordered_map<uint32_t, high_resolution_clock::time_point> pending_seqs_;
    std::mutex pending_mutex_;

    high_resolution_clock::time_point connect_start_;
    std::atomic<bool> connected_{false};
    std::string receiver_uid_;
};

static std::vector<UserToken> load_tokens(const std::string& path) {
    std::vector<UserToken> tokens;
    std::ifstream f(path);
    if (!f) { std::cerr << "Cannot open " << path << std::endl; return tokens; }
    json j; f >> j;
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

static void fmt_sample_sink(std::ostream& os, const std::string& label, SampleSink& s, bool csv) {
    size_t n = s.size();
    if (n < 2) {
        os << (csv ? "" : "  ") << label << (csv ? "," : ": ") << "no_samples" << std::endl;
        return;
    }
    std::vector<double> v(s.samples, s.samples + n);
    std::sort(v.begin(), v.end());
    double sum = std::accumulate(v.begin(), v.end(), 0.0);
    double avg = sum / n;
    double p50 = v[static_cast<size_t>(n * 0.50)];
    double p90 = v[static_cast<size_t>(n * 0.90)];
    double p95 = v[static_cast<size_t>(n * 0.95)];
    double p99 = v[static_cast<size_t>(n * 0.99)];
    double min = v.front();
    double max = v.back();

    if (csv) {
        os << label << "_count," << n << std::endl;
        os << label << "_min," << std::fixed << std::setprecision(2) << min << std::endl;
        os << label << "_avg," << avg << std::endl;
        os << label << "_max," << max << std::endl;
        os << label << "_p50," << p50 << std::endl;
        os << label << "_p90," << p90 << std::endl;
        os << label << "_p95," << p95 << std::endl;
        os << label << "_p99," << p99 << std::endl;
        if (s.dropped.load())
            os << label << "_dropped," << s.dropped.load() << std::endl;
    } else {
        os << "  " << label << ":" << std::endl;
        os << "    count: " << n;
        if (s.dropped.load()) os << " (dropped: " << s.dropped.load() << ")";
        os << std::endl;
        os << "    min/avg/max: " << std::fixed << std::setprecision(2) << min << " / " << avg << " / " << max << " ms" << std::endl;
        os << "    p50/p90/p95/p99: " << p50 << " / " << p90 << " / " << p95 << " / " << p99 << " ms" << std::endl;
    }
}

static void print_stats(bool csv, int duration_sec) {
    std::ostringstream oss;

    int64_t sent = g_stats.msgs_sent.load();
    int64_t recv = g_stats.msgs_recv.load();
    int64_t ok = g_stats.connects_ok.load();
    int64_t fail = g_stats.connects_fail.load();

    if (csv) {
        oss << "connects_ok," << ok << std::endl;
        oss << "connects_fail," << fail << std::endl;
        oss << "connects_attempted," << (ok + fail) << std::endl;
        oss << "messages_sent," << sent << std::endl;
        oss << "messages_recv," << recv << std::endl;
        oss << "errors," << g_stats.errors.load() << std::endl;
        oss << "bytes_sent," << g_stats.bytes_sent.load() << std::endl;
        oss << "bytes_recv," << g_stats.bytes_recv.load() << std::endl;
        oss << "duration_sec," << duration_sec << std::endl;
        oss << "throughput_msg_s," << std::fixed << std::setprecision(2) << (double)sent / duration_sec << std::endl;
        oss << "throughput_recv_s," << (double)recv / duration_sec << std::endl;
        oss << "throughput_kbps," << std::fixed << std::setprecision(2) << (g_stats.bytes_sent.load() / 1000.0) / duration_sec << std::endl;
        fmt_sample_sink(oss, "connect_time", g_stats.connect_time_ms, true);
        fmt_sample_sink(oss, "rtt", g_stats.rtt_ms, true);
    } else {
        oss << "\n======= Results =======" << std::endl;
        oss << "Connects OK:    " << ok << std::endl;
        oss << "Connects FAIL:  " << fail << std::endl;
        oss << "Connects total: " << (ok + fail) << std::endl;
        oss << "Messages sent:  " << sent << std::endl;
        oss << "Messages recv:  " << recv << std::endl;
        oss << "Errors:         " << g_stats.errors.load() << std::endl;
        oss << "Bytes sent:     " << g_stats.bytes_sent.load() << std::endl;
        oss << "Bytes recv:     " << g_stats.bytes_recv.load() << std::endl;
        oss << "Duration:       " << duration_sec << "s" << std::endl;
        oss << "Throughput:     " << std::fixed << std::setprecision(1) << (double)sent / duration_sec << " msg/s";
        oss << "  (" << std::setprecision(2) << (g_stats.bytes_sent.load() / 1000.0) / duration_sec << " KB/s)" << std::endl;
        fmt_sample_sink(oss, "Connect time", g_stats.connect_time_ms, false);
        fmt_sample_sink(oss, "RTT", g_stats.rtt_ms, false);
    }

    std::cout << oss.str();
}

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
        else if (arg == "--timeout" && i + 1 < argc) cfg.conn_timeout_ms = std::stoi(argv[++i]);
        else if (arg == "--connect-rate" && i + 1 < argc) cfg.connect_rate = std::stoi(argv[++i]);
        else if (arg == "--threads" && i + 1 < argc) cfg.threads = std::stoi(argv[++i]);
        else if (arg == "--csv") cfg.csv_mode = true;
        else if (arg == "--help") {
            std::cout << "Usage: bench_ws [options]\n"
                      << "  --host HOST             Server host (default: 127.0.0.1)\n"
                      << "  --port PORT             Server port (default: 10001)\n"
                      << "  --tokens FILE           Token JSON file\n"
                      << "  --users N               Max users to connect (0=all)\n"
                      << "  --duration SEC          Test duration (default: 60)\n"
                      << "  --interval MS           Message interval ms (default: 5000)\n"
                      << "  --timeout MS            Connection timeout (default: 15000)\n"
                      << "  --connect-rate N        Connections per second (default: unlimited)\n"
                      << "  --threads N             IO threads (default: 4)\n"
                      << "  --csv                   CSV output mode\n";
            return 0;
        }
    }

    im::utils::LogManager::SetLoggingEnabled("protobuf_codec", false);

    auto tokens = load_tokens(cfg.token_file);
    if (tokens.empty()) { std::cerr << "No tokens loaded." << std::endl; return 1; }
    if (cfg.max_users > 0 && static_cast<size_t>(cfg.max_users) < tokens.size())
        tokens.resize(cfg.max_users);

    std::cerr << "Users: " << tokens.size()
              << " | Interval: " << cfg.message_interval_ms << "ms"
              << " | Duration: " << cfg.duration_sec << "s"
              << " | Threads: " << cfg.threads
              << " | Connect rate: " << (cfg.connect_rate > 0 ? std::to_string(cfg.connect_rate) + "/s" : "unlimited")
              << std::endl;

    net::io_context ioc;
    ssl::context ssl_ctx(ssl::context::tlsv12_client);
    ssl_ctx.set_default_verify_paths();
    ssl_ctx.set_verify_mode(ssl::verify_none);

    int connect_rate = cfg.connect_rate > 0 ? cfg.connect_rate : std::max(10, (int)tokens.size() / 5);
    int stagger_us = connect_rate > 0 ? 1000000 / connect_rate : 0;

    std::vector<std::shared_ptr<WsSession>> sessions;
    for (size_t i = 0; i < tokens.size(); ++i) {
        auto& t = tokens[i];
        auto& next_t = tokens[(i + 1) % tokens.size()];
        auto s = std::make_shared<WsSession>(ioc, ssl_ctx, cfg.conn_timeout_ms);
        s->set_host(cfg.host);
        s->set_port(cfg.port);
        s->set_token(t);
        s->set_receiver_uid(next_t.uid);
        s->set_interval_ms(cfg.message_interval_ms);
        sessions.push_back(s);
    }

    auto start_ts = high_resolution_clock::now();
    for (size_t i = 0; i < sessions.size(); ++i) {
        sessions[i]->start();
        if (stagger_us > 0 && i + 1 < sessions.size())
            std::this_thread::sleep_for(microseconds(stagger_us));
    }

    std::vector<std::thread> workers;
    for (int i = 0; i < cfg.threads; ++i)
        workers.emplace_back([&ioc] { ioc.run(); });

    std::this_thread::sleep_for(seconds(cfg.duration_sec));

    g_stop.store(true);

    for (auto& s : sessions) s->stop();
    ioc.stop();
    for (auto& w : workers) {
        if (w.joinable()) w.join();
    }

    auto elapsed = duration_cast<milliseconds>(high_resolution_clock::now() - start_ts).count() / 1000.0;
    std::cerr << "Elapsed: " << std::fixed << std::setprecision(1) << elapsed << "s" << std::endl;

    print_stats(cfg.csv_mode, (int)elapsed);

    return g_stats.connects_fail.load() > 0 && g_stats.connects_ok.load() == 0 ? 1 : 0;
}
