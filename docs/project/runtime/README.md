# 运行与验证入口

本目录用于保存当前项目的运行、部署、测试验证文档。

当前详细历史记录已归档到：

- `docs/history/devlog/phase18_remote_runtime_runbook.md`
- `docs/history/devlog/phase18_release_closure_checklist.md`
- `docs/history/devlog/phase18_stabilization.md`

后续需要在本目录重写当前版运行文档，避免继续依赖历史 devlog。

## 当前本地验证入口

远程服务全栈：

```bash
scripts/dev/run_remote_services_stack.sh
```

Web 验证客户端：

```bash
cd clients/web
npm run dev -- --host 127.0.0.1
```

默认地址：

```text
http://127.0.0.1:5173/
```

Gateway 默认端口：

- HTTP: `127.0.0.1:8102`
- WebSocket TLS: `127.0.0.1:8101`

## 待重写文档

- 单机 MVP 启动与测试手册。
- Web 双账号验证流程。
- remote-all 服务拓扑说明。
- 多机器部署验证方案。
- 常见故障排查手册。
