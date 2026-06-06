#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${repo_root}"

start_docker="${MYCHAT_DEV_START_DOCKER:-auto}"
run_migrations="${MYCHAT_DEV_RUN_MIGRATIONS:-ON}"

is_enabled() {
  case "$1" in
    ON|on|1|true|TRUE|yes|YES)
      return 0
      ;;
  esac
  return 1
}

is_disabled() {
  case "$1" in
    OFF|off|0|false|FALSE|no|NO)
      return 0
      ;;
  esac
  return 1
}

wait_for_container() {
  local container="$1"
  echo "==> Waiting for ${container} to become healthy"
  for _ in {1..60}; do
    health="$(docker inspect --format='{{if .State.Health}}{{.State.Health.Status}}{{else}}none{{end}}' "${container}" 2>/dev/null || true)"
    if [[ "${health}" == "healthy" || "${health}" == "none" ]]; then
      return
    fi
    sleep 2
  done

  health="$(docker inspect --format='{{if .State.Health}}{{.State.Health.Status}}{{else}}none{{end}}' "${container}" 2>/dev/null || true)"
  echo "Container ${container} did not become healthy; current health=${health}" >&2
  exit 1
}

ensure_docker_dependencies() {
  if is_disabled "${start_docker}"; then
    echo "==> Skipping Docker dependency startup because MYCHAT_DEV_START_DOCKER=${start_docker}"
    return
  fi

  if command -v docker >/dev/null 2>&1 && docker compose version >/dev/null 2>&1; then
    echo "==> Starting Redis/PostgreSQL development dependencies"
    docker compose up -d redis postgres
    wait_for_container mychat-redis
    wait_for_container mychat-postgres
    return
  fi

  if is_enabled "${start_docker}"; then
    cat >&2 <<MSG
Docker Compose is required to start dependencies automatically, but it is not available.

Start compatible services manually or set:
  MYCHAT_DEV_START_DOCKER=OFF scripts/dev/prepare_runtime.sh

Expected development defaults:
  Redis:      127.0.0.1:6379, password mychat-dev-pass
  PostgreSQL: 127.0.0.1:5432, db/user/password mychat/mychat/mychat-dev-pass
MSG
    exit 2
  fi

  cat >&2 <<MSG
docker compose is not available; assuming Redis/PostgreSQL are already running.

Expected development defaults:
  Redis:      127.0.0.1:6379, password mychat-dev-pass
  PostgreSQL: 127.0.0.1:5432, db/user/password mychat/mychat/mychat-dev-pass
MSG
}

ensure_docker_dependencies

if is_disabled "${run_migrations}"; then
  echo "==> Skipping PostgreSQL migrations because MYCHAT_DEV_RUN_MIGRATIONS=${run_migrations}"
else
  echo "==> Applying PostgreSQL schema migrations"
  scripts/db/migrate_postgres.sh
fi

echo "==> MyChat local runtime dependencies are ready"
