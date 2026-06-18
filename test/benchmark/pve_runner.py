#!/usr/bin/env python3
"""Run commands on the PVE load generator host via SSH with password."""
import pexpect
import sys
import os

PVE_HOST = "192.168.1.185"
PVE_USER = "root"
PVE_PASS = "123123123"
SSH_OPTS = "-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"

def ssh(cmd, timeout=30):
    full_cmd = f"ssh {SSH_OPTS} {PVE_USER}@{PVE_HOST} '{cmd}'"
    child = pexpect.spawn(full_cmd, timeout=timeout, encoding='utf-8')
    i = child.expect_exact(["password: ", pexpect.TIMEOUT, pexpect.EOF], timeout=8)
    if i == 1:
        # Check if already authenticated
        child.expect(pexpect.EOF, timeout=5)
        return child.before or ""
    elif i == 0:
        child.sendline(PVE_PASS)
        child.expect(pexpect.EOF, timeout=timeout)
        return child.before or ""
    else:
        # Already authenticated (key auth worked)
        return child.before or ""

def scp(src, dst, timeout=30):
    full_cmd = f"scp {SSH_OPTS} {src} {PVE_USER}@{PVE_HOST}:{dst}"
    child = pexpect.spawn(full_cmd, timeout=timeout, encoding='utf-8')
    i = child.expect_exact(["password: ", pexpect.TIMEOUT, pexpect.EOF], timeout=8)
    if i == 0:
        child.sendline(PVE_PASS)
        child.expect(pexpect.EOF, timeout=timeout)
    return child.before or ""

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: pve_runner.py <command> [args...]")
        sys.exit(1)
    
    cmd = " ".join(sys.argv[1:])
    result = ssh(cmd, timeout=int(os.environ.get("PVE_TIMEOUT", "60")))
    print(result.strip())
