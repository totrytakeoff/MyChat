#!/usr/bin/env python3
"""MyChat 压测 SSH 工具库。

通过 pexpect 管理远程命令执行和文件传输。

用法:
  from pve_runner import RemoteHost
  host = RemoteHost("user@10.0.0.1", password="")  # 密码为空则用密钥
  host.run("ls /tmp")
  host.upload("local_file", "/remote/path")
"""
import pexpect
import os

SSH_OPTS = "-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"

class RemoteHost:
    def __init__(self, host_str, password=None):
        """
        host_str: "user@host" 或 "host" (默认用当前用户)
        password: SSH 密码, None/"" 表示使用密钥认证
        """
        self.host = host_str
        self.password = password

    def run(self, cmd, timeout=120):
        """执行命令并返回 stdout。"""
        c = f"ssh {SSH_OPTS} {self.host} '{cmd}'"
        child = pexpect.spawn(c, timeout=timeout, encoding='utf-8')
        idx = child.expect_exact(["password: ", pexpect.EOF], timeout=10)
        if idx == 0:
            if not self.password:
                raise RuntimeError("SSH requires password but none provided")
            child.sendline(self.password)
            child.expect(pexpect.EOF, timeout=timeout)
        return child.before or ""

    def upload(self, local_path, remote_path, timeout=30):
        """上传文件到远端。"""
        c = f"scp -o StrictHostKeyChecking=no {local_path} {self.host}:{remote_path}"
        child = pexpect.spawn(c, timeout=timeout)
        idx = child.expect_exact(["password: ", pexpect.EOF], timeout=10)
        if idx == 0:
            if not self.password:
                raise RuntimeError("SSH requires password but none provided")
            child.sendline(self.password)
            child.expect(pexpect.EOF, timeout=timeout)

    def download(self, remote_path, local_path, timeout=30):
        """从远端下载文件。"""
        c = f"scp -o StrictHostKeyChecking=no {self.host}:{remote_path} {local_path}"
        child = pexpect.spawn(c, timeout=timeout)
        idx = child.expect_exact(["password: ", pexpect.EOF], timeout=10)
        if idx == 0:
            if not self.password:
                raise RuntimeError("SSH requires password but none provided")
            child.sendline(self.password)
            child.expect(pexpect.EOF, timeout=timeout)


if __name__ == "__main__":
    # 示例用法: 从环境变量读取
    pve_host = os.environ.get("BENCH_PVE_HOST", "")
    pve_user = os.environ.get("BENCH_PVE_USER", "root")
    pve_pass = os.environ.get("BENCH_PVE_PASS", "")

    if pve_host:
        host = RemoteHost(f"{pve_user}@{pve_host}", pve_pass or None)
        print(host.run("uptime"))
    else:
        print("请设置 BENCH_PVE_HOST 环境变量")
