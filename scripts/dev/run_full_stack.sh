#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${repo_root}"

log_dir="${MYCHAT_DEV_LOG_DIR:-build/dev-runtime}"
web_host="${MYCHAT_WEB_HOST:-127.0.0.1}"
web_port="${MYCHAT_WEB_PORT:-5173}"

backend_pid=""
web_pid=""

shutdown() {
  set +e
  if [[ -n "${web_pid}" ]] && kill -0 "${web_pid}" 2>/dev/null; then
    echo "==> Stopping web client (${web_pid})"
    kill "${web_pid}" 2>/dev/null || true
  fi
  if [[ -n "${backend_pid}" ]] && kill -0 "${backend_pid}" 2>/dev/null; then
    echo "==> Stopping backend stack (${backend_pid})"
    kill "${backend_pid}" 2>/dev/null || true
  fi
  if [[ -n "${web_pid}" ]]; then
    wait "${web_pid}" 2>/dev/null || true
  fi
  if [[ -n "${backend_pid}" ]]; then
    wait "${backend_pid}" 2>/dev/null || true
  fi
}

require_file() {
  local path="$1"
  if [[ ! -f "${path}" ]]; then
    echo "Required file is missing: ${path}" >&2
    exit 1
  fi
}

require_file "clients/web/package.json"
require_file "scripts/dev/run_remote_services_stack.sh"

mkdir -p "${log_dir}"
trap shutdown EXIT HUP INT TERM

echo "==> Starting backend remote-all stack"
scripts/dev/run_remote_services_stack.sh >"${log_dir}/full_stack_backend.log" 2>&1 &
backend_pid="$!"

sleep 2
if ! kill -0 "${backend_pid}" 2>/dev/null; then
  echo "Backend stack exited during startup; log follows:" >&2
  sed -n '1,180p' "${log_dir}/full_stack_backend.log" >&2 || true
  exit 1
fi

echo "==> Starting Web client"
(
  cd clients/web
  exec npm run dev -- --host "${web_host}" --port "${web_port}"
) >"${log_dir}/full_stack_web.log" 2>&1 &
web_pid="$!"

sleep 2
if ! kill -0 "${web_pid}" 2>/dev/null; then
  echo "Web client exited during startup; log follows:" >&2
  sed -n '1,180p' "${log_dir}/full_stack_web.log" >&2 || true
  exit 1
fi

cat <<MSG
==> MyChat full development stack is running
  logs:       ${log_dir}
  backend:    ${log_dir}/full_stack_backend.log
  web:        ${log_dir}/full_stack_web.log

Open:
  Web UI:     http://${web_host}:${web_port}/
  Gateway:    http://127.0.0.1:8102/api/v1/health

Press Ctrl+C to stop frontend and backend.
MSG

wait -n "${backend_pid}" "${web_pid}"
exit_code="$?"
echo "==> One process exited; shutting down full development stack"
exit "${exit_code}"
