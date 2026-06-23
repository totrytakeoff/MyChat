# 项目架构文档

这里放当前仍代表 MyChat 现状的架构类文档。

## 当前文档

- `mvp_architecture.md`：当前项目整体架构说明。
- `web_validation_client_plan.md`：Web 验证客户端规划与交互边界。
- `interview_review_plan.md`：面向总结、面试和多机验证的阅读与测试路径。
- `gateway_unified_message_refactor_baseline.md`：Gateway 回归统一消息处理架构的现状、目标、约束和验收基准。
- `gateway_handler_boundary_refactor_requirements.md`：Gateway handler/client/service 职责边界纠偏要求。
- `gateway_protobuf_packet_boundary_baseline.md`：Gateway 与各微服务之间 protobuf packet boundary 的硬性职责边界。

## 阅读顺序

1. `mvp_architecture.md`
2. `web_validation_client_plan.md`
3. `interview_review_plan.md`
4. `gateway_unified_message_refactor_baseline.md`
5. `gateway_handler_boundary_refactor_requirements.md`
6. `gateway_protobuf_packet_boundary_baseline.md`

历史阶段日志和旧参考资料请转到 `docs/history/` 或 `docs/reference/`。
