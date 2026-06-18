#!/bin/bash
# Full benchmark runner with monitoring
# Usage: run_benchmark.sh [--users N] [--duration N] [--interval N] [--no-wss] [--no-http]

set -e

CLOUD_HOST="122.51.70.33"
PVE_HOST="root@192.168.1.185"
PVE_PASS="123123123"
CLOUD_SSH="ssh ASD-Host"
PVE_SSH="sshpass -p $PVE_PASS ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $PVE_HOST"

USERS=100
DURATION=120
INTERVAL=5000
RUN_WSS=true
RUN_HTTP=true
REPORT_DIR="/tmp/benchmark/reports"

# Parse args
while [[ $# -gt 0 ]]; do
    case "$1" in
        --users) USERS="$2"; shift 2 ;;
        --duration) DURATION="$2"; shift 2 ;;
        --interval) INTERVAL="$2"; shift 2 ;;
        --no-wss) RUN_WSS=false; shift ;;
        --no-http) RUN_HTTP=false; shift ;;
        *) echo "Unknown: $1"; exit 1 ;;
    esac
done

mkdir -p "$REPORT_DIR"
REPORT="$REPORT_DIR/benchmark_$(date +%Y%m%d_%H%M%S).txt"

echo "============================================" | tee -a "$REPORT"
echo " MyChat Benchmark Report" | tee -a "$REPORT"
echo " Date: $(date '+%Y-%m-%d %H:%M:%S')" | tee -a "$REPORT"
echo " Cloud: $CLOUD_HOST (SUT)" | tee -a "$REPORT"
echo " LoadGen: PVE (i7-13700K/64GB)" | tee -a "$REPORT"
echo " Users: $USERS, Duration: ${DURATION}s" | tee -a "$REPORT"
echo "============================================" | tee -a "$REPORT"
echo "" | tee -a "$REPORT"

# Function to collect system stats
collect_stats() {
    local label="$1"
    local outfile="$2"
    echo "=== $label ===" >> "$outfile"
    echo "TIME: $(date '+%Y-%m-%d %H:%M:%S')" >> "$outfile"

    # Cloud server stats
    echo "--- Cloud Server ($CLOUD_HOST) ---" >> "$outfile"
    $CLOUD_SSH "echo 'CPU:'; top -bn1 | head -5; echo 'MEM:'; free -h; echo 'PROC:'; ps aux | grep gateway_server | grep -v grep | awk '{printf \"PID=%s CPU=%s%% MEM=%s%% RSS=%s\\n\", \$2, \$3, \$4, \$6}'" 2>/dev/null >> "$outfile"

    # PVE stats  
    echo "--- PVE LoadGen ---" >> "$outfile"
    top -bn1 2>/dev/null | head -5 >> "$outfile" 2>/dev/null || true
    free -h >> "$outfile"
}

# Initial state
echo "=== Baseline State ===" | tee -a "$REPORT"
$CLOUD_SSH "free -h && uptime && ps aux | grep gateway_server | grep -v grep" 2>/dev/null | tee -a "$REPORT"

# ---- WSS Benchmark ----
if $RUN_WSS; then
    echo "" | tee -a "$REPORT"
    echo "============================================" | tee -a "$REPORT"
    echo " WSS Benchmark (bench_ws)" | tee -a "$REPORT"
    echo " Users: $USERS, Interval: ${INTERVAL}ms" | tee -a "$REPORT"
    echo "============================================" | tee -a "$REPORT"

    # Pre-collect stats
    collect_stats "WSS_BEFORE" "$REPORT"

    START_TIME=$(date +%s)
    # Run bench_ws on PVE
    $PVE_SSH "/tmp/benchmark/bench_ws --host $CLOUD_HOST --port 10001 --tokens /tmp/benchmark/users.json --duration $DURATION --interval $INTERVAL --users $USERS" 2>/dev/null | tee -a "$REPORT"

    # Post-collect stats
    collect_stats "WSS_AFTER" "$REPORT"
    echo "" | tee -a "$REPORT"
fi

# ---- HTTP Benchmark ----
if $RUN_HTTP; then
    echo "" | tee -a "$REPORT"
    echo "============================================" | tee -a "$REPORT"
    echo " HTTP Benchmark (k6)" | tee -a "$REPORT"
    echo "============================================" | tee -a "$REPORT"

    # Collect pre-stats
    collect_stats "HTTP_BEFORE" "$REPORT"

    # Run k6 on PVE
    $PVE_SSH "/tmp/benchmark/k6 run --vus $USERS --duration ${DURATION}s -e BASE_URL=http://$CLOUD_HOST:10002 /tmp/benchmark/http_benchmark.js" 2>/dev/null | tee -a "$REPORT"

    collect_stats "HTTP_AFTER" "$REPORT"
    echo "" | tee -a "$REPORT"
fi

# ---- Final cloud stats ----
echo "" | tee -a "$REPORT"
echo "=== Final Cloud Server State ===" | tee -a "$REPORT"
$CLOUD_SSH "free -h && uptime && cat /proc/loadavg" 2>/dev/null | tee -a "$REPORT"

echo "" | tee -a "$REPORT"
echo "Report saved to: $REPORT" | tee -a "$REPORT"
