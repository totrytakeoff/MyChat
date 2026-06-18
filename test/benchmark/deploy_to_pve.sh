#!/bin/bash
set -e
PVE_HOST=myself@192.168.1.185
PVE_PASS=123123123

export DISPLAY=:0
SSH_ASKPASS_REQUIRE=force
SSH_ASKPASS=/tmp/sshpass.sh

cat > /tmp/sshpass.sh << 'ASSEOF'
#!/bin/bash
echo "123123123"
ASSEOF
chmod +x /tmp/sshpass.sh

BENCH_DIR=/home/myself/workspace/MyChat/test/benchmark
BUILD_DIR=/home/myself/workspace/MyChat/build/benchmark

echo "=== Copying benchmark files to PVE host ==="
scp -o StrictHostKeyChecking=no \
  "$BUILD_DIR/bench_ws" \
  "$BENCH_DIR/prep_users.py" \
  "$BENCH_DIR/http_benchmark.js" \
  "$BENCH_DIR/CMakeLists.txt" \
  "$BENCH_DIR/bench_ws.cpp" \
  "$BENCH_DIR/protobuf_codec.cpp" \
  "$PVE_HOST:/tmp/benchmark/"

echo "=== Setting up PVE host ==="
ssh -o StrictHostKeyChecking=no "$PVE_HOST" "
set -e
cd /tmp/benchmark

# Check k6
which k6 || { echo 'k6 not found, installing...' && sudo apt-get update -qq && sudo apt-get install -y -qq k6 2>/dev/null || (curl -LO https://github.com/grafana/k6/releases/download/v0.54.0/k6-v0.54.0-linux-amd64.deb && sudo dpkg -i k6-v0.54.0-linux-amd64.deb); }

echo '=== Testing connectivity to cloud server ==='
curl -s --connect-timeout 5 http://122.51.70.33:10002/api/v1/health
echo ''
echo '=== k6 version ==='
k6 version
echo '=== bench_ws binary ==='
file ./bench_ws
echo 'All set up!'
"

rm -f /tmp/sshpass.sh
