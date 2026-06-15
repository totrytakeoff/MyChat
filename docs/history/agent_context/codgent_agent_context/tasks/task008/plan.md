---
task_id: task008
type: plan
status: ready_for_implementation
from: planner
to: coder
revision: 0
requires_review: true
---

# Task 008 Plan: Extract PushService with FanoutPolicy

## Status

Ready for implementation.

## Owner

Implementation Agent.

## Reviewer

Planner/Reviewer Agent.

## Context

Task 003 completed the Message Service persistence core. Task 004 completed
Gateway Message HTTP integration. Task 006 completed Gateway WebSocket
`CMD_SEND_MESSAGE` send/ack. Task 007 completed online delivery: after
successful persistence, `MessageWsHandler::try_push_to_recipient` pushes a
`CMD_PUSH_MESSAGE` protobuf to the recipient's active WebSocket sessions and
marks the message delivered.

The push logic is currently embedded in `MessageWsHandler` as a private
`try_push_to_recipient` method. This couples message handling to push delivery
and makes the push path difficult to test independently. A dedicated
`PushService` with a pluggable `FanoutPolicy` separates concerns, enables
independent testing, and sets up for future multi-recipient fanout and a
standalone Push microservice.

## Goal

Extract the push logic from `MessageWsHandler::try_push_to_recipient` into a
dedicated `PushService` class in `gateway/` with a `FanoutPolicy` abstraction.
`MessageWsHandler` delegates to `PushService::push_to_user` instead of
owning the push loop. The default `FanoutPolicy` pushes to all active sessions
(current behavior). The `PushService` is independently testable.
