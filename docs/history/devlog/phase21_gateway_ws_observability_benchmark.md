# Phase 21: Gateway WSS Observability And Benchmark Cleanup

Date: 2026-06-21

## Background

After the WSS lifecycle/heartbeat fixes, Gateway no longer kept large
`CLOSE-WAIT` / `ESTAB` leftovers after the benchmark idle window. The remaining
problem was WSS connection setup:

- `conn-10/50/100` passed after lifecycle cleanup.
- `conn-200` still stabilized at about `125/200`.
- Existing `bench_ws` output could not separate client-side scheduling delay,
  TCP connect delay, TLS handshake delay, and WebSocket upgrade delay.

This phase focused on measurement correctness and observability before changing
Gateway's accept/IO model.

## Implemented

### 1. `bench_ws` measurement fix

`test/benchmark/bench_ws.cpp` no longer schedules all sessions before starting
`io_context` workers.

Old model:

```text
for each session:
  session.start()
  sleep(stagger)
start io_context workers
```

New model:

```text
create io_context work guard
start io_context workers
for each session:
  post session.start()
  sleep(stagger)
```

This avoids counting local ramp-up sleep as connection latency.

The tool now records:

- total connect time
- resolve time
- TCP connect time
- TLS handshake time
- WebSocket handshake time
- failure classification: resolve/tcp/ssl/ws/timeout/post-connect

### 2. Benchmark-local codec

`test/benchmark/benchmark_codec.hpp` and `.cpp` were added so the standalone
benchmark binary does not need to link the full business-side
`common/network/protobuf_codec.cpp` and logging stack.

`test/benchmark/CMakeLists.txt` now supports:

- normal builds using checked-in generated protobuf sources
- Debian 12 compatible builds with `MYCHAT_BENCH_REGENERATE_PROTO=ON`

This solved the previous `GLIBC_2.38` incompatibility on the PVE pressure host.

### 3. Gateway WSS handshake observability

`WebSocketServer` now tracks:

- accept ok/fail count
- active handshakes
- current sessions
- SSL handshake count/avg/max
- HTTP upgrade read count/avg/max
- WebSocket accept count/avg/max
- session add count/avg/max

`WebSocketSession` records stage timings around:

- server-side TLS handshake
- HTTP upgrade request read
- WebSocket accept
- server session registration

`GatewayServer::get_server_stats()` prints these counters.

`GET /api/v1/stats` was added as a read-only runtime observability endpoint.
It is registered before the catch-all route so benchmark runs can fetch Gateway
stats directly.

## Verification

Local build and tests:

```bash
cmake --build build/benchmark --target bench_ws -j2
cmake --build build/remote-push-odb --target gateway_server test_gateway_message_ws -j2
ctest --test-dir build/remote-push-odb -R GatewayMessageWsTest --output-on-failure
git diff --check
```

Results:

```text
bench_ws built successfully
gateway_server built successfully
GatewayMessageWsTest passed
git diff --check passed
```

Debian 12 benchmark build:

```bash
cmake -S test/benchmark -B /tmp/mychat-bench-build \
  -DCMAKE_BUILD_TYPE=Release \
  -DPROJECT_ROOT=/src \
  -DMYCHAT_BENCH_REGENERATE_PROTO=ON
cmake --build /tmp/mychat-bench-build --target bench_ws -j2
```

Generated binary hash used on the pressure host:

```text
0986f6755264ed053ca4308c6b4ab2fd615be63a52b4fff32e4dc0d98057e379
```

Gateway binary hash deployed for the observability retest:

```text
1291248247ab837764c1768410087b9c5caee825510e7e991d43209bd396bdb4
```

## Benchmark Findings

Report:

- `docs/benchmark/benchmark_report_20260621_ws_observe/benchmark_report_20260621_ws_observe.md`

Key results:

- PVE public-network pressure test `100 users @ 20/s`: `100/100`.
- PVE public-network pressure test `150 users @ 20/s`: `125/150`.
- PVE public-network pressure test `200 users @ 20/s`: `125/200`.
- Raising the PVE shell `ulimit -n` to `65535` did not change `150 users`:
  still `125/150`.
- SUT local loopback test `150 users @ 20/s`: `150/150`, connect avg `2.80ms`.

Gateway stats after tests showed that connections which entered the application
layer completed TLS/upgrade/session registration normally:

```text
ws.accept_fail: 0
ws.ssl_handshake.avg_ms: 38.8091
ws.upgrade_read.avg_ms: 31.7531
ws.accept_handshake.avg_ms: 0.0754
ws.session_add.avg_ms: 0.7977
ws.inflight_messages: 0
```

Conclusion:

The current `125` public-network threshold is not caused by protobuf encoding,
message persistence, push fanout, WSS close lifecycle, Gateway fd limit, or the
old benchmark scheduling bug. The strongest current hypothesis is a TCP/public
network ingress limit or packet loss/retry behavior between the PVE pressure
host and the cloud SUT, possibly provider-side single-source-IP connection
rate/concurrency protection.

## Follow-up

Next WSS benchmark work should focus on network-path validation:

1. Capture `netstat -s` deltas before and after each run.
2. Run the same matrix from another public host and from same-cloud/internal
   network.
3. Use `tcpdump` on the SUT for `tcp port 10001` to inspect SYN/SYN-ACK/ACK
   behavior for timed-out connections.
4. Test `net.ipv4.tcp_max_syn_backlog=4096/8192` on SUT.
5. Expand `connect-rate 5/10/20/40` with `--timeout 30s`.
6. Teach benchmark scripts to archive `/api/v1/stats`, `ss`, `netstat -s`,
   and `/proc/<pid>/limits` automatically.
