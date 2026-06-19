#!/bin/bash
# 部署压测工具到发压端
# 使用前: source env.sh (复制 env.sh.example 并填入实际值)

set -euo pipefail

: "${BENCH_PVE_USER:?需要设置 BENCH_PVE_USER}"
: "${BENCH_PVE_HOST:?需要设置 BENCH_PVE_HOST}"
: "${BENCH_PVE_PASS:?需要设置 BENCH_PVE_PASS}"
: "${BENCH_PVE_TOOL_DIR:?需要设置 BENCH_PVE_TOOL_DIR}"

PVE="${BENCH_PVE_USER}@${BENCH_PVE_HOST}"
SSH_OPTS="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"
SCP="sshpass -p ${BENCH_PVE_PASS} scp ${SSH_OPTS}"
SSH="sshpass -p ${BENCH_PVE_PASS} ssh ${SSH_OPTS} ${PVE}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== 部署到 ${PVE}:${BENCH_PVE_TOOL_DIR} ==="

# 1. 创建目录
$SSH "mkdir -p ${BENCH_PVE_TOOL_DIR}"

# 2. 上传 bench_ws
$SCP "${SCRIPT_DIR}/build_deb12/bench_ws" "${PVE}:${BENCH_PVE_TOOL_DIR}/bench_ws"
echo "  bench_ws ✓"

# 3. 上传 k6 脚本
$SCP "${SCRIPT_DIR}/http_benchmark.js" "${PVE}:${BENCH_PVE_TOOL_DIR}/"
echo "  http_benchmark.js ✓"

# 4. 上传 prep_users.py
$SCP "${SCRIPT_DIR}/prep_users.py" "${PVE}:${BENCH_PVE_TOOL_DIR}/"
echo "  prep_users.py ✓"

# 5. 检查 k6 (需要单独安装)
echo ""
echo "如果 k6 未安装在 ${BENCH_PVE_TOOL_DIR}/k6, 请在发压端执行:"
echo "  cp /usr/local/bin/k6 ${BENCH_PVE_TOOL_DIR}/k6"
echo "  或"
echo "  wget https://github.com/grafana/k6/releases/latest/download/k6-linux-amd64.tar.gz -O - | tar xz && mv k6 ${BENCH_PVE_TOOL_DIR}/k6"

# 6. 验证
echo ""
$SSH "ls -lh ${BENCH_PVE_TOOL_DIR}/"
echo ""
echo "=== 部署完成 ==="
