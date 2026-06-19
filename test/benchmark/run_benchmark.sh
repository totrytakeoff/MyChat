#!/bin/bash
# MyChat Benchmark 一键运行
# 使用前: cp env.sh.example env.sh && vim env.sh && source env.sh

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

if [ ! -f "${SCRIPT_DIR}/env.sh" ]; then
    echo "请先创建 env.sh: cp env.sh.example env.sh && vim env.sh"
    exit 1
fi

source "${SCRIPT_DIR}/env.sh"

echo "=== MyChat 压测 ==="
echo "目标:  wss://${BENCH_CLOUD_HOST}:${BENCH_CLOUD_WSS_PORT:-10001}"
echo "      http://${BENCH_CLOUD_HOST}:${BENCH_CLOUD_HTTP_PORT:-10002}"
echo "发压端: ${BENCH_PVE_USER}@${BENCH_PVE_HOST}"
echo ""

python3 "${SCRIPT_DIR}/run_all.py" "$@"
