#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${repo_root}"

build_dir="${MYCHAT_DEV_REMOTE_PUSH_BUILD_DIR:-build/remote-push-odb}"
config_file="${MYCHAT_DEV_REMOTE_PUSH_CONFIG:-config/dev.remote-push.json}"
push_server_bin="${MYCHAT_DEV_PUSH_SERVER_BIN:-${build_dir}/services/push/push_server}"
gateway_server_bin="${MYCHAT_DEV_GATEWAY_SERVER_BIN:-${build_dir}/gateway/gateway_server}"
log_dir="${MYCHAT_DEV_LOG_DIR:-build/dev-runtime}"

if [[ ! -x "${push_server_bin}" || ! -x "${gateway_server_bin}" ]]; then
  cat >&2 <<MSG
Remote Push development binaries were not found.

Expected:
  ${push_server_bin}
  ${gateway_server_bin}

Build the ODB + Push gRPC development tree first, for example:
  scripts/ci/remote_push_odb.sh

Or override the binary paths:
  MYCHAT_DEV_PUSH_SERVER_BIN=/path/to/push_server \\
  MYCHAT_DEV_GATEWAY_SERVER_BIN=/path/to/gateway_server \\
  scripts/dev/run_remote_push_stack.sh
MSG
  exit 2
fi

if [[ ! -f "${config_file}" ]]; then
  echo "Config file does not exist: ${config_file}" >&2
  exit 1
fi

scripts/dev/prepare_runtime.sh

mkdir -p "${log_dir}"
push_log="${log_dir}/push_server.log"
gateway_log="${log_dir}/gateway_server.log"

push_pid=""
gateway_pid=""

shutdown() {
  set +e
  if [[ -n "${gateway_pid}" ]] && kill -0 "${gateway_pid}" 2>/dev/null; then
    kill "${gateway_pid}" 2>/dev/null || true
  fi
  if [[ -n "${push_pid}" ]] && kill -0 "${push_pid}" 2>/dev/null; then
    kill "${push_pid}" 2>/dev/null || true
  fi
  wait "${gateway_pid}" 2>/dev/null || true
  wait "${push_pid}" 2>/dev/null || true
}

trap shutdown EXIT INT TERM

echo "==> Starting push_server with ${config_file}"
"${push_server_bin}" --config "${config_file}" >"${push_log}" 2>&1 &
push_pid="$!"

sleep 1
if ! kill -0 "${push_pid}" 2>/dev/null; then
  echo "push_server exited during startup; log follows:" >&2
  sed -n '1,160p' "${push_log}" >&2 || true
  exit 1
fi

echo "==> Starting gateway_server with ${config_file}"
"${gateway_server_bin}" --config "${config_file}" >"${gateway_log}" 2>&1 &
gateway_pid="$!"

sleep 1
if ! kill -0 "${gateway_pid}" 2>/dev/null; then
  echo "gateway_server exited during startup; log follows:" >&2
  sed -n '1,160p' "${gateway_log}" >&2 || true
  exit 1
fi

cat <<MSG
==> Remote Push development stack is running
  push_server pid:    ${push_pid}
  gateway_server pid: ${gateway_pid}
  push log:           ${push_log}
  gateway log:        ${gateway_log}

Gateway endpoints from config/dev.remote-push.json:
  HTTP:      http://127.0.0.1:8102/api/v1/health
  WebSocket: wss://127.0.0.1:8101

Press Ctrl+C to stop both processes.
MSG

wait -n "${push_pid}" "${gateway_pid}"
exit_code="$?"
echo "==> One process exited; shutting down remote Push development stack"
exit "${exit_code}"
