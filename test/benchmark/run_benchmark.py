#!/usr/bin/env python3
"""Full MyChat benchmark runner with monitoring."""
import argparse
import json
import os
import subprocess
import sys
import time
import pexpect
from datetime import datetime

CLOUD_HOST = "122.51.70.33"
PVE_HOST = "192.168.1.185"
PVE_USER = "root"
PVE_PASS = "123123123"
BENCH_DIR = "/tmp/benchmark"
SSH_OPTS = "-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"

def ssh_pve(cmd, timeout=120):
    full = f"ssh {SSH_OPTS} {PVE_USER}@{PVE_HOST} '{cmd}'"
    child = pexpect.spawn(full, timeout=timeout, encoding='utf-8')
    i = child.expect_exact(["password: ", pexpect.EOF, pexpect.TIMEOUT], timeout=10)
    if i == 0:
        child.sendline(PVE_PASS)
        child.expect(pexpect.EOF, timeout=timeout)
    return child.before or ""

def ssh_cloud(cmd, timeout=30):
    r = subprocess.run(
        ["ssh", "ASD-Host"] + cmd.split(),
        capture_output=True, text=True, timeout=timeout
    )
    return r.stdout

def collect_cloud_stats():
    out = ssh_cloud("free -h")
    out += "\n" + ssh_cloud("uptime")
    out += "\n" + ssh_cloud("cat /proc/loadavg")
    out += "\n" + ssh_cloud("ps aux | grep gateway_server | grep -v grep | awk '{printf \"PID=%s CPU=%s%% MEM=%s%% RSS=%s\\n\", $2, $3, $4, $6}'")
    out += "\n" + ssh_cloud("ss -s")
    out += "\n" + ssh_cloud("cat /proc/net/tcp | wc -l")
    return out

def collect_pve_stats():
    out = ssh_pve("free -h")
    out += "\n" + ssh_pve("uptime")
    out += "\n" + ssh_pve("cat /proc/loadavg")
    return out

def prep_users(host, port, count, output):
    print(f"Preparing {count} users on {host}:{port}...")
    out = ssh_pve(
        f"python3 {BENCH_DIR}/prep_users.py --host {host} --port {port} -n {count} --password pass1234 -o {output}",
        timeout=600
    )
    print(out[-500:] if len(out) > 500 else out)
    lines = [l for l in out.split('\n') if 'Wrote' in l]
    if lines:
        print(f"  -> {lines[-1]}")

def run_wss(users, duration, interval, report_file):
    print(f"\n{'='*60}")
    print(f"WSS Benchmark: {users} users, {duration}s, {interval}ms interval")
    print(f"{'='*60}")

    with open(report_file, 'a') as f:
        f.write(f"\n=== WSS {users}u/{duration}s/{interval}ms ===\n")
        f.write("--- CLOUD BEFORE ---\n")
        f.write(collect_cloud_stats())
        f.write("--- PVE BEFORE ---\n")
        f.write(collect_pve_stats())

    start = time.time()
    out = ssh_pve(
        f"{BENCH_DIR}/bench_ws --host {CLOUD_HOST} --port 10001 "
        f"--tokens {BENCH_DIR}/users.json --duration {duration} "
        f"--interval {interval} --users {users}",
        timeout=duration + 60
    )
    elapsed = time.time() - start

    with open(report_file, 'a') as f:
        f.write(out)
        f.write("\n--- AFTER ---\n")
        f.write("--- CLOUD AFTER ---\n")
        f.write(collect_cloud_stats())
        f.write("--- PVE AFTER ---\n")
        f.write(collect_pve_stats())
        f.write(f"\nElapsed: {elapsed:.1f}s\n")

    # Extract key metrics for display
    for line in out.split('\n'):
        line = line.strip()
        if any(k in line for k in ['Results', 'Connects OK', 'Connects FAIL', 'Messages sent',
                                     'Messages recv', 'Errors', 'Bytes sent', 'Bytes recv',
                                     'RTT', 'count:', 'min/', 'p50/']):
            print(f"  {line}")
    print()

def run_http(vus, duration, report_file):
    print(f"\n{'='*60}")
    print(f"HTTP Benchmark: {vus} VUs, {duration}s")
    print(f"{'='*60}")

    with open(report_file, 'a') as f:
        f.write(f"\n=== HTTP {vus}vu/{duration}s ===\n")
        f.write("--- CLOUD BEFORE ---\n")
        f.write(collect_cloud_stats())
        f.write("--- PVE BEFORE ---\n")
        f.write(collect_pve_stats())

    start = time.time()
    out = ssh_pve(
        f"{BENCH_DIR}/k6 run --vus {vus} --duration {duration}s "
        f"-e BASE_URL=http://{CLOUD_HOST}:10002 "
        f"{BENCH_DIR}/http_benchmark.js",
        timeout=duration + 60
    )
    elapsed = time.time() - start

    with open(report_file, 'a') as f:
        f.write(out)
        f.write("--- AFTER ---\n")
        f.write("--- CLOUD AFTER ---\n")
        f.write(collect_cloud_stats())
        f.write("--- PVE AFTER ---\n")
        f.write(collect_pve_stats())
        f.write(f"\nElapsed: {elapsed:.1f}s\n")

    # Print summary
    lines = out.split('\n')
    for line in lines[-30:]:
        line = line.strip()
        if line and not line.startswith('running'):
            print(f"  {line}")
    print()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--users", type=int, default=200, help="Number of test users")
    ap.add_argument("--wss-users", nargs='+', type=int, default=[10, 50, 100, 200],
                    help="WSS concurrency levels")
    ap.add_argument("--http-vus", nargs='+', type=int, default=[10, 50, 100],
                    help="HTTP concurrency levels")
    ap.add_argument("--duration", type=int, default=60, help="Test duration per run")
    ap.add_argument("--interval", type=int, default=5000, help="WS message interval (ms)")
    ap.add_argument("--no-wss", action='store_true')
    ap.add_argument("--no-http", action='store_true')
    ap.add_argument("--no-prep", action='store_true', help="Skip user preparation")
    args = ap.parse_args()

    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    report_file = f"/tmp/benchmark/report_{ts}.txt"
    os.makedirs("/tmp/benchmark", exist_ok=True)

    with open(report_file, 'w') as f:
        f.write(f"MyChat Benchmark Report\n")
        f.write(f"Date: {datetime.now().isoformat()}\n")
        f.write(f"Cloud SUT: {CLOUD_HOST} (Debian 13, 4c4g)\n")
        f.write(f"LoadGen: PVE {PVE_HOST} (i7-13700K/64GB)\n")
        f.write(f"Target: WSS port 10001, HTTP port 10002\n")
        f.write(f"{'='*60}\n\n")

    print(f"Report: {report_file}")

    # Check cloud gateway is running
    health = ssh_cloud("curl -s --connect-timeout 5 http://127.0.0.1:10002/api/v1/health")
    if not health:
        print("ERROR: Cloud gateway not responding!")
        sys.exit(1)
    print(f"Gateway health: {health[:80]}")

    # Prep users
    if not args.no_prep:
        need = max(max(args.wss_users or [0]), max(args.http_vus or [0]), args.users)
        print(f"\nNeed at least {need} users, preparing {args.users}...")
        prep_users(CLOUD_HOST, 10002, args.users, f"{BENCH_DIR}/users.json")

    # WSS benchmark
    if not args.no_wss:
        for users in args.wss_users:
            run_wss(users, args.duration, args.interval, report_file)

    # HTTP benchmark
    if not args.no_http:
        for vus in args.http_vus:
            run_http(vus, args.duration, report_file)

    print(f"\n{'='*60}")
    print(f"Benchmark complete! Report: {report_file}")
    print(f"{'='*60}")

    # Print final summary from report
    with open(report_file) as f:
        content = f.read()
    print("\n--- WSS Summary ---")
    for line in content.split('\n'):
        if '=== WSS' in line or 'Connects OK' in line or 'Connects FAIL' in line or 'Errors' in line or 'RTT:' in line or ('min/' in line and 'avg' in line and 'p50' in line):
            print(f"  {line.strip()}")
    print("\n--- HTTP Summary ---")
    for line in content.split('\n'):
        if '=== HTTP' in line or 'http_req_duration' in line or 'http_reqs' in line or 'info_duration' in line or 'http_req_failed' in line or 'request_failed' in line:
            print(f"  {line.strip()}")

if __name__ == '__main__':
    main()
