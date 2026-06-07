#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${repo_root}"

build_dir="${MYCHAT_DEV_REMOTE_SERVICES_BUILD_DIR:-build/remote-push-odb}"
config_file="${MYCHAT_DEV_REMOTE_SERVICES_CONFIG:-config/dev.remote-all.json}"
log_dir="${MYCHAT_DEV_LOG_DIR:-build/dev-runtime}"

user_server_bin="${MYCHAT_DEV_USER_SERVER_BIN:-${build_dir}/services/user/user_server}"
message_server_bin="${MYCHAT_DEV_MESSAGE_SERVER_BIN:-${build_dir}/services/message/message_server}"
friend_server_bin="${MYCHAT_DEV_FRIEND_SERVER_BIN:-${build_dir}/services/friend/friend_server}"
group_server_bin="${MYCHAT_DEV_GROUP_SERVER_BIN:-${build_dir}/services/group/group_server}"
push_server_bin="${MYCHAT_DEV_PUSH_SERVER_BIN:-${build_dir}/services/push/push_server}"
gateway_server_bin="${MYCHAT_DEV_GATEWAY_SERVER_BIN:-${build_dir}/gateway/gateway_server}"

declare -a processes=()

require_executable() {
  local path="$1"
  if [[ ! -x "${path}" ]]; then
    echo "Required binary is missing or not executable: ${path}" >&2
    return 1
  fi
}

shutdown() {
  set +e
  for entry in "${processes[@]}"; do
    local name="${entry%%:*}"
    local pid="${entry#*:}"
    if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
      echo "==> Stopping ${name} (${pid})"
      kill "${pid}" 2>/dev/null || true
    fi
  done
  for entry in "${processes[@]}"; do
    local pid="${entry#*:}"
    wait "${pid}" 2>/dev/null || true
  done
}

start_process() {
  local name="$1"
  local bin="$2"
  local log_file="${log_dir}/${name}.log"

  echo "==> Starting ${name}"
  "${bin}" --config "${config_file}" >"${log_file}" 2>&1 &
  local pid="$!"
  processes+=("${name}:${pid}")

  sleep 1
  if ! kill -0 "${pid}" 2>/dev/null; then
    echo "${name} exited during startup; log follows:" >&2
    sed -n '1,160p' "${log_file}" >&2 || true
    exit 1
  fi
}

for bin in \
  "${user_server_bin}" \
  "${message_server_bin}" \
  "${friend_server_bin}" \
  "${group_server_bin}" \
  "${push_server_bin}" \
  "${gateway_server_bin}"; do
  require_executable "${bin}"
done

if [[ ! -f "${config_file}" ]]; then
  echo "Config file does not exist: ${config_file}" >&2
  exit 1
fi

scripts/dev/prepare_runtime.sh
mkdir -p "${log_dir}"

trap shutdown EXIT HUP INT TERM

start_process "user_server" "${user_server_bin}"
start_process "message_server" "${message_server_bin}"
start_process "friend_server" "${friend_server_bin}"
start_process "group_server" "${group_server_bin}"
start_process "push_server" "${push_server_bin}"
start_process "gateway_server" "${gateway_server_bin}"

cat <<MSG
==> Remote services development stack is running
  config:  ${config_file}
  logs:    ${log_dir}

Services:
  user_server:    127.0.0.1:9001
  message_server: 127.0.0.1:9002
  friend_server:  127.0.0.1:9003
  group_server:   127.0.0.1:9004
  push_server:    127.0.0.1:9101

Gateway:
  HTTP:      http://127.0.0.1:8102/api/v1/health
  WebSocket: wss://127.0.0.1:8101
  Push callback: 127.0.0.1:9102

Press Ctrl+C to stop all processes.
MSG

wait -n "${processes[@]#*:}"
exit_code="$?"
echo "==> One process exited; shutting down remote services development stack"
exit "${exit_code}"
