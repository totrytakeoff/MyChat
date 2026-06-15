# Phase 18: Release Closure Checklist

Date: 2026-06-07

## Current Release Candidate Baseline

Latest committed baseline:

```text
e34ba0e Stabilize remote services runtime
```

The active MVP baseline is stable locally:

- Default no-ODB/no-gRPC regression: `scripts/ci/default_regression.sh`, 2/2
  tests passed.
- Remote-services ODB/gRPC full suite:
  `ctest --test-dir build/remote-push-odb --output-on-failure --parallel 1`,
  40/40 tests passed.
- Remote Gateway focused smokes passed for User, Message, Friend, Group, and
  Push.
- Full local remote runtime smoke passed through
  `scripts/dev/run_remote_services_stack.sh` plus Gateway health.
- Worktree was clean after commit and post-commit tests.

## Active Baseline

Treat these as the supported local entrypoints:

- `scripts/ci/checks.sh`
- `scripts/ci/default_regression.sh`
- `scripts/ci/remote_push_odb.sh`
- `scripts/dev/prepare_runtime.sh`
- `scripts/dev/run_remote_push_stack.sh`
- `scripts/dev/run_remote_services_stack.sh`

Treat these configs as active:

- `config/dev.json` for default local facade mode.
- `config/dev.remote-push.json` for Gateway + remote Push.
- `config/dev.remote-all.json` for local all-service remote mode.

## Must Preserve

- Default builds remain ODB-off/gRPC-off unless explicitly configured.
- All generated protobuf/gRPC outputs come from CMake `generate_proto`.
- Heavy ODB/gRPC CTest remains serial: `--parallel 1`.
- Service repositories stay on direct `odb::pgsql::database` unless a dedicated
  repository-boundary task migrates them to `PgSqlConnection`.
- Gateway HTTP/WS external contracts stay compatible with the current tests.
- Hosted GitHub CI remains paused unless explicitly reintroduced.

## Known Deferred Work

These are not release blockers for the current local MVP, but they should stay
visible:

- Stronger ODB test data isolation before parallel CTest.
- Hosted GitHub CI reintroduction after local stabilization remains boring.
- Redis pool sizing/load benchmark for quantified defaults.
- Production TLS/certificate/secret management.
- Deployment packaging and operational process management.
- Legacy test cleanup behind `MYCHAT_BUILD_LEGACY_*` switches.
- Optional cleanup of `SendRequest::msg_type` default semantics for
  `send_text_message`.

## Final Pre-Commit Gate

Before the next commit that changes runtime or tests:

```bash
git diff --check
scripts/ci/checks.sh
scripts/ci/default_regression.sh
ctest --test-dir build/remote-push-odb --output-on-failure --parallel 1
```

If protobuf or gRPC contracts changed, use the scripted path instead:

```bash
scripts/ci/remote_push_odb.sh
```

## Next Work Recommendation

Do not start new product features until the current release-candidate docs and
handoff remain consistent after one more clean read-through. The next useful
engineering tasks are:

1. Add test data isolation for ODB suites if parallel CI is desired.
2. Reintroduce hosted CI in small steps: checks first, then default regression,
   then heavy regression as manual or scheduled only.
3. Add a small runtime smoke script that starts the all-service stack, checks
   health, and exits cleanly.
