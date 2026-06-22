#!/usr/bin/env python3
"""MyChat 全面压测执行器。

用法:
  # 设置环境变量（参考 env.sh.example）
  export BENCH_CLOUD_HOST=10.0.0.1
  export BENCH_CLOUD_SSH=my-cloud-ssh
  export BENCH_PVE_HOST=10.0.0.2
  # 推荐使用 SSH 密钥认证
  export BENCH_PVE_PASS=""   # 仅在必要时

  # 只跑 WSS
  python3 run_all.py --wss-only

  # 只跑 HTTP
  python3 run_all.py --http-only

  # 自定义测试阶段
  python3 run_all.py --phases conn,light,medium

环境变量:
  BENCH_CLOUD_HOST      被测服务器 IP/域名
  BENCH_CLOUD_WSS_PORT  WSS 端口 (默认 10001)
  BENCH_CLOUD_HTTP_PORT HTTP 端口 (默认 10002)
  BENCH_CLOUD_SSH       被测服务器 SSH 连接串 (user@host 或别名)
  BENCH_PVE_HOST        发压端 IP/域名
  BENCH_PVE_USER        发压端 SSH 用户 (默认 root)
  BENCH_PVE_PASS        发压端 SSH 密码 (推荐用密钥)
  BENCH_PVE_TOOL_DIR    发压端工具目录 (默认 /tmp/benchmark)
"""
import argparse
import pexpect
import subprocess
import time
import os
import sys
from datetime import datetime
from pathlib import Path

# ---- 从环境变量读取配置 ----
CLOUD_HOST  = os.environ.get("BENCH_CLOUD_HOST", "")
CLOUD_SSH   = os.environ.get("BENCH_CLOUD_SSH", "")
CLOUD_WSS   = int(os.environ.get("BENCH_CLOUD_WSS_PORT", "10001"))
CLOUD_HTTP  = int(os.environ.get("BENCH_CLOUD_HTTP_PORT", "10002"))
PVE_HOST    = os.environ.get("BENCH_PVE_HOST", "")
PVE_USER    = os.environ.get("BENCH_PVE_USER", "root")
PVE_PASS    = os.environ.get("BENCH_PVE_PASS", "")
PVE_DIR     = os.environ.get("BENCH_PVE_TOOL_DIR", "/tmp/benchmark")
PVE_SSH     = f"{PVE_USER}@{PVE_HOST}"

BIN       = f"{PVE_DIR}/bench_ws"
TOKENS    = f"{PVE_DIR}/users.json"
K6        = f"{PVE_DIR}/k6"
K6_SCRIPT = f"{PVE_DIR}/http_benchmark.js"
OUTDIR    = f"{PVE_DIR}/logs"

def _check_config():
    missing = []
    if not CLOUD_HOST: missing.append("BENCH_CLOUD_HOST")
    if not CLOUD_SSH:  missing.append("BENCH_CLOUD_SSH")
    if not PVE_HOST:   missing.append("BENCH_PVE_HOST")
    if missing:
        print(f"缺少环境变量: {', '.join(missing)}")
        print("请复制 env.sh.example 为 env.sh 并填入实际值, 然后 source env.sh")
        sys.exit(1)

def pve(cmd, timeout=180):
    """在发压端执行命令。"""
    c = f"ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null {PVE_SSH} '{cmd}'"
    child = pexpect.spawn(c, timeout=timeout, encoding='utf-8')
    idx = child.expect_exact(["password: ", pexpect.EOF], timeout=timeout)
    if idx == 0:
        if not PVE_PASS:
            print("    [?] SSH 需要密码但 BENCH_PVE_PASS 为空")
            sys.exit(1)
        child.sendline(PVE_PASS)
        child.expect(pexpect.EOF, timeout=timeout)
    return child.before or ""

def cloud(cmd, timeout=30):
    """在被测服务器执行命令。"""
    if " " in CLOUD_SSH:
        # user@host 格式, 需要解析
        parts = CLOUD_SSH.split()
        r = subprocess.run(["ssh"] + parts + cmd.split(),
                           capture_output=True, text=True, timeout=timeout)
    else:
        r = subprocess.run(["ssh", CLOUD_SSH] + cmd.split(),
                           capture_output=True, text=True, timeout=timeout)
    return r.stdout.strip()

def save(path, content):
    Path(path).write_text(content)

# ========== WSS 测试 ==========
def wss_test(users, duration, interval, tag):
    print(f"\n{'='*50}")
    print(f"WSS {tag}: users={users} dur={duration}s int={interval}ms")
    print(f"{'='*50}")

    pre = cloud("uptime; free -h; cat /proc/loadavg")
    save(f"{OUTDIR}/{TS}_{tag}_pre.txt", pre)
    load = pre.split('\n')[-1] if pre else 'N/A'
    print(f"  pre-load: {load}")

    interval_flag = str(interval) if interval > 0 else "999999"
    cmd = (f"{BIN} --host {CLOUD_HOST} --port {CLOUD_WSS} "
           f"--tokens {TOKENS} --duration {duration} --interval {interval_flag} "
           f"--users {users} --threads 4 --connect-rate 20 2>&1")
    out = pve(cmd, timeout=duration + 120)
    save(f"{OUTDIR}/{TS}_{tag}.txt", out)

    time.sleep(2)
    post = cloud("uptime; free -h; cat /proc/loadavg")
    save(f"{OUTDIR}/{TS}_{tag}_post.txt", post)
    load = post.split('\n')[-1] if post else 'N/A'
    print(f"  post-load: {load}")

    for line in out.split('\n'):
        line = line.strip()
        if any(k in line for k in ['Connects OK', 'Connects FAIL',
                                     'Messages sent', 'Messages recv', 'Errors']):
            print(f"  {line}")
    for line in out.split('\n'):
        if 'min/avg/max:' in line or 'p50/p90/p95/p99:' in line:
            print(f"  {line.strip()}")
    return out

# ========== HTTP 测试 ==========
def http_ramp():
    tag = "HTTP-Ramp"
    print(f"\n{'='*50}")
    print(f"HTTP: staged ramp 10->50->100->200->500 VUs")
    print(f"{'='*50}")

    pre = cloud("uptime; free -h; cat /proc/loadavg")
    save(f"{OUTDIR}/{TS}_{tag}_pre.txt", pre)

    out = pve(f"cd {PVE_DIR} && {K6} run -e BASE_URL=http://{CLOUD_HOST}:{CLOUD_HTTP} {K6_SCRIPT} 2>&1",
              timeout=300)
    save(f"{OUTDIR}/{TS}_{tag}.txt", out)

    time.sleep(2)
    post = cloud("uptime; free -h; cat /proc/loadavg")
    save(f"{OUTDIR}/{TS}_{tag}_post.txt", post)

    for line in out.split('\n'):
        s = line.strip()
        if any(k in s for k in ['http_req_duration', 'http_reqs', 'http_req_failed',
                                 'request_failed', 'info_duration', 'health_duration']):
            print(f"  {s}")
    return out

# ========== Main ==========
if __name__ == '__main__':
    _check_config()

    ap = argparse.ArgumentParser()
    ap.add_argument("--wss-only", action="store_true")
    ap.add_argument("--http-only", action="store_true")
    ap.add_argument("--phases", default="all",
                    help="逗号分隔: all, conn, light, medium, stress")
    args = ap.parse_args()

    run_wss = not args.http_only
    run_http = not args.wss_only
    phases = set(args.phases.split(",")) if args.phases != "all" else {"all"}
    all_phases = "all" in phases

    TS = datetime.now().strftime("%Y%m%d_%H%M%S")
    os.makedirs(OUTDIR, exist_ok=True)

    print(f"日志目录: {OUTDIR}/{TS}_*")
    print(f"目标: wss://{CLOUD_HOST}:{CLOUD_WSS}  http://{CLOUD_HOST}:{CLOUD_HTTP}")
    print(f"发压端: {PVE_SSH}")
    if not PVE_PASS:
        print(f"认证方式: SSH 密钥")
    print()

    # Baseline
    print("--- 基线 ---")
    bl = cloud("uptime; free -h; ps aux | grep gateway_server | grep -v grep "
               "| awk '{print \"PID=\"$2\" CPU=\"$3\"% MEM=\"$4\"%\"}'")
    print(bl)
    save(f"{OUTDIR}/{TS}_baseline.txt", bl)

    if run_wss:
        if all_phases or "conn" in phases:
            print("\n========== 阶段1: 连接压力 ==========")
            wss_test(10, 30, 0, "conn-10")
            wss_test(50, 30, 0, "conn-50")
            wss_test(100, 30, 0, "conn-100")
            wss_test(200, 30, 0, "conn-200")

        if all_phases or "light" in phases:
            print("\n========== 阶段2: 轻量消息吞吐 ==========")
            wss_test(10, 60, 10000, "ws-10u-10s")
            wss_test(50, 60, 10000, "ws-50u-10s")
            wss_test(100, 60, 5000, "ws-100u-5s")

        if all_phases or "medium" in phases:
            print("\n========== 阶段3: 中量消息吞吐 ==========")
            wss_test(50, 60, 5000, "ws-50u-5s")
            wss_test(100, 60, 2000, "ws-100u-2s")
            wss_test(200, 60, 5000, "ws-200u-5s")

        if all_phases or "stress" in phases:
            print("\n========== 阶段4: 压力测试 ==========")
            wss_test(100, 60, 1000, "ws-100u-1s")
            wss_test(200, 60, 1000, "ws-200u-1s")
            wss_test(200, 30, 500, "ws-200u-500ms")
            wss_test(200, 30, 250, "ws-200u-250ms")

    if run_http:
        print("\n========== 阶段5: HTTP 阶梯压测 ==========")
        http_ramp()

    # Final
    print("\n--- 结束 ---")
    fl = cloud("uptime; free -h; ps aux | grep gateway_server | grep -v grep")
    print(fl)
    save(f"{OUTDIR}/{TS}_final.txt", fl)
    print(f"\n所有日志已保存到 {OUTDIR}/{TS}_*")
    print("完成。")
