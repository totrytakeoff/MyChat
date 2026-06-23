# Web 验证客户端说明

## 当前定位

Web 客户端位于：

```text
clients/web
```

它是用于验证 MyChat 后端 MVP 的中文 IM 风格客户端，不是后台管理面板，也不是最终商业客户端。

当前目标：

- 让后端能力能被真实用户流程验证。
- 用正常 IM 交互隐藏内部 uid、device、endpoint 等调试细节。
- 保留 debug drawer 方便查看 HTTP/WS 事件。
- 为后续 Electron、Qt 客户端提供交互模型参考。

## 当前可验证能力

- 注册/登录。
- 个人资料查看和编辑。
- 用户搜索。
- 好友申请和处理。
- 好友列表。
- 单聊消息发送。
- 单聊实时推送。
- 群创建。
- 群搜索和加入。
- 群资料和成员展示。
- 群消息发送。
- 群消息实时推送。
- 未读红点。
- Enter / Ctrl+Enter 发送设置。

## 与后端的关系

Web 客户端只访问 Gateway：

```text
Web Client
-> Gateway HTTP / WebSocket
-> Gateway MessageParser / UnifiedMessage
-> GatewayRuntimeRegistry
-> local service packet dispatcher 或 remote ForwardPacket gRPC
-> 后端服务
```

Web 客户端不关心 Gateway 后面是本地服务还是远程 gRPC 服务。

默认开发入口：

```text
http://127.0.0.1:5173/
```

默认代理：

```text
/api/* -> Gateway HTTP 127.0.0.1:8102
/ws    -> Gateway WebSocket TLS 127.0.0.1:8101
```

## 当前边界

当前 Web 客户端用于验证后端，不追求完整产品化。

已避免的问题：

- 登录页不暴露 endpoint/device/platform。
- 用户添加先搜索再申请。
- 群聊加入先搜索再进入资料页。
- 内部 uid 不作为主要用户识别信息。
- 401 等认证问题应引导回登录状态。

仍属于后续增强：

- 更完善的个人主页和群资料视觉 polish。
- 更完整的错误提示体系。
- 自动化 Playwright 测试入库。
- 富媒体消息 UI。
- 通知设置持久化。

## 面试可讲点

- 为什么后端项目需要一个 Web 验证客户端。
- 为什么 Web 只访问 Gateway。
- 如何验证 WebSocket 二进制 protobuf 消息。
- 如何用双窗口验证在线推送。
- Web 客户端和未来 Electron/Qt 客户端的关系。
