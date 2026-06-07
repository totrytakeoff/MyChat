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
ctest_args=(--output-on-failure --parallel 1)

regenerate_proto() {
  echo "==> Removing stale protobuf/gRPC outputs"
  cmake -E rm -f \
    common/proto/base.pb.cc common/proto/base.pb.h \
    common/proto/command.pb.cc common/proto/command.pb.h \
    common/proto/user.pb.cc common/proto/user.pb.h \
    common/proto/friend.pb.cc common/proto/friend.pb.h \
    common/proto/group.pb.cc common/proto/group.pb.h \
    common/proto/message.pb.cc common/proto/message.pb.h \
    common/proto/push.pb.cc common/proto/push.pb.h \
    common/proto/codec_service.pb.cc common/proto/codec_service.pb.h \
    common/proto/user.grpc.pb.cc common/proto/user.grpc.pb.h \
    common/proto/friend.grpc.pb.cc common/proto/friend.grpc.pb.h \
    common/proto/group.grpc.pb.cc common/proto/group.grpc.pb.h \
    common/proto/message.grpc.pb.cc common/proto/message.grpc.pb.h \
    common/proto/push.grpc.pb.cc common/proto/push.grpc.pb.h \
    common/proto/codec_service.grpc.pb.cc common/proto/codec_service.grpc.pb.h

  echo "==> Regenerating protobuf/gRPC sources with the configured toolchain"
  cmake --build "${build_dir}" --target generate_proto -j"${jobs}"
}

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

Build it before running the remote services ODB/gRPC CI slice:
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

echo "==> Applying PostgreSQL schema migrations"
scripts/db/migrate_postgres.sh

echo "==> Configuring remote services ODB/gRPC build: ${build_dir}"
cmake -S . -B "${build_dir}" \
  -DVCPKG_INSTALLED_DIR="${vcpkg_installed_dir}" \
  -DVCPKG_MANIFEST_FEATURES="pgsql-odb;codec-grpc" \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_USER_GRPC_SERVICE=ON \
  -DMYCHAT_BUILD_MESSAGE_GRPC_SERVICE=ON \
  -DMYCHAT_BUILD_FRIEND_GRPC_SERVICE=ON \
  -DMYCHAT_BUILD_GROUP_GRPC_SERVICE=ON \
  -DMYCHAT_BUILD_PUSH_GRPC_SERVICE=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DMYCHAT_ODB_ROOT="${odb_root}" \
  -DCMAKE_BUILD_TYPE="${build_type}"

regenerate_proto

echo "==> Building full remote Push Gateway smoke target"
cmake --build "${build_dir}" --target test_remote_push_gateway_server_smoke -j"${jobs}"

echo "==> Running full Gateway-server remote Push smoke"
ctest --test-dir "${build_dir}" -R RemotePushGatewayServerSmokeTest "${ctest_args[@]}"

echo "==> Building all registered remote services ODB/gRPC test targets"
cmake --build "${build_dir}" -j"${jobs}"

echo "==> Running Push-focused tests serially"
ctest --test-dir "${build_dir}" -R Push "${ctest_args[@]}"

echo "==> Running full remote services ODB/gRPC test suite serially"
ctest --test-dir "${build_dir}" "${ctest_args[@]}"

echo "==> Remote services ODB/gRPC CI passed"
