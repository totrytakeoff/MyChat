#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${repo_root}"

build_dir="${MYCHAT_CI_REMOTE_PUSH_BUILD_DIR:-build/ci-remote-push-odb}"
jobs="${MYCHAT_CI_JOBS:-2}"
build_type="${MYCHAT_CI_BUILD_TYPE:-Debug}"
odb_root="${MYCHAT_ODB_ROOT:-${repo_root}/.odb/installed}"
build_odb_runtime="${MYCHAT_CI_BUILD_ODB_RUNTIME:-OFF}"
vcpkg_installed_dir="${MYCHAT_CI_VCPKG_INSTALLED_DIR:-${repo_root}/vcpkg_installed/remote-push-odb}"

if [[ ! -f "${odb_root}/lib/libodb.a" ]]; then
  if [[ "${build_odb_runtime}" == "ON" || "${build_odb_runtime}" == "1" || "${build_odb_runtime}" == "true" ]]; then
    echo "==> ODB 2.5.0 runtime missing; building it through scripts/build_odb_runtime_2_5.sh"
    scripts/build_odb_runtime_2_5.sh --prefix "${odb_root}"
  fi
fi

if [[ ! -f "${odb_root}/lib/libodb.a" ]]; then
  cat >&2 <<MSG
ODB 2.5.0 runtime was not found at:
  ${odb_root}

Build it before running the remote Push ODB/gRPC CI slice:
  scripts/build_odb_runtime_2_5.sh

Or provide an existing install prefix:
  MYCHAT_ODB_ROOT=/path/to/odb-2.5.0 scripts/ci/remote_push_odb.sh
MSG
  exit 2
fi

if command -v docker >/dev/null 2>&1 && docker compose version >/dev/null 2>&1; then
  echo "==> Starting Redis/PostgreSQL dependencies with docker compose"
  docker compose up -d redis postgres
  for container in mychat-redis mychat-postgres; do
    echo "==> Waiting for ${container} to become healthy"
    for _ in {1..60}; do
      health="$(docker inspect --format='{{if .State.Health}}{{.State.Health.Status}}{{else}}none{{end}}' "${container}" 2>/dev/null || true)"
      if [[ "${health}" == "healthy" || "${health}" == "none" ]]; then
        break
      fi
      sleep 2
    done
    health="$(docker inspect --format='{{if .State.Health}}{{.State.Health.Status}}{{else}}none{{end}}' "${container}" 2>/dev/null || true)"
    if [[ "${health}" != "healthy" && "${health}" != "none" ]]; then
      echo "Container ${container} did not become healthy; current health=${health}" >&2
      exit 1
    fi
  done
else
  cat >&2 <<MSG
docker compose is not available. Start compatible Redis/PostgreSQL services manually.

Expected development defaults:
  Redis:      127.0.0.1:6379, password mychat-dev-pass
  PostgreSQL: 127.0.0.1:5432, db/user/password mychat/mychat/mychat-dev-pass
MSG
fi

echo "==> Configuring remote Push ODB/gRPC build: ${build_dir}"
cmake -S . -B "${build_dir}" \
  -DVCPKG_INSTALLED_DIR="${vcpkg_installed_dir}" \
  -DVCPKG_MANIFEST_FEATURES="pgsql-odb;codec-grpc" \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PUSH_GRPC_SERVICE=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DMYCHAT_ODB_ROOT="${odb_root}" \
  -DCMAKE_BUILD_TYPE="${build_type}"

echo "==> Building full remote Push Gateway smoke target"
cmake --build "${build_dir}" --target test_remote_push_gateway_server_smoke -j"${jobs}"

echo "==> Running full Gateway-server remote Push smoke"
ctest --test-dir "${build_dir}" -R RemotePushGatewayServerSmokeTest --output-on-failure

echo "==> Building all registered remote Push ODB/gRPC test targets"
cmake --build "${build_dir}" -j"${jobs}"

echo "==> Running Push-focused tests"
ctest --test-dir "${build_dir}" -R Push --output-on-failure

echo "==> Running full remote Push ODB/gRPC test suite"
ctest --test-dir "${build_dir}" --output-on-failure

echo "==> Remote Push ODB/gRPC CI passed"
