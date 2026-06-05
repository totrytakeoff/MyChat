#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${repo_root}"

build_dir="${MYCHAT_CI_DEFAULT_BUILD_DIR:-build/ci-default}"
jobs="${MYCHAT_CI_JOBS:-2}"
build_type="${MYCHAT_CI_BUILD_TYPE:-Debug}"
vcpkg_installed_dir="${MYCHAT_CI_VCPKG_INSTALLED_DIR:-${repo_root}/vcpkg_installed/default}"
start_redis="${MYCHAT_CI_START_REDIS:-auto}"

regenerate_common_proto() {
  echo "==> Removing stale protobuf outputs"
  cmake -E rm -f \
    common/proto/base.pb.cc common/proto/base.pb.h \
    common/proto/command.pb.cc common/proto/command.pb.h \
    common/proto/user.pb.cc common/proto/user.pb.h \
    common/proto/friend.pb.cc common/proto/friend.pb.h \
    common/proto/group.pb.cc common/proto/group.pb.h \
    common/proto/message.pb.cc common/proto/message.pb.h \
    common/proto/push.pb.cc common/proto/push.pb.h \
    common/proto/codec_service.pb.cc common/proto/codec_service.pb.h

  echo "==> Regenerating protobuf sources with the configured toolchain"
  cmake --build "${build_dir}" --target generate_common_proto -j"${jobs}"
}

ensure_redis_dependency() {
  case "${start_redis}" in
    OFF|off|0|false|FALSE)
      echo "==> Skipping Redis startup because MYCHAT_CI_START_REDIS=${start_redis}"
      return
      ;;
  esac

  if command -v docker >/dev/null 2>&1 && docker compose version >/dev/null 2>&1; then
    echo "==> Starting Redis dependency with docker compose"
    docker compose up -d redis

    echo "==> Waiting for mychat-redis to become healthy"
    for _ in {1..60}; do
      health="$(docker inspect --format='{{if .State.Health}}{{.State.Health.Status}}{{else}}none{{end}}' mychat-redis 2>/dev/null || true)"
      if [[ "${health}" == "healthy" || "${health}" == "none" ]]; then
        return
      fi
      sleep 2
    done

    health="$(docker inspect --format='{{if .State.Health}}{{.State.Health.Status}}{{else}}none{{end}}' mychat-redis 2>/dev/null || true)"
    echo "Redis container did not become healthy; current health=${health}" >&2
    exit 1
  fi

  if [[ "${start_redis}" == "ON" || "${start_redis}" == "on" || "${start_redis}" == "1" || "${start_redis}" == "true" || "${start_redis}" == "TRUE" ]]; then
    cat >&2 <<MSG
Docker Compose is required to start Redis automatically, but it is not available.

Start a compatible Redis service manually or set:
  MYCHAT_CI_START_REDIS=OFF scripts/ci/default_regression.sh

Expected development default:
  Redis: 127.0.0.1:6379, password mychat-dev-pass
MSG
    exit 2
  fi

  cat >&2 <<MSG
docker compose is not available; assuming Redis is already running.

Expected development default:
  Redis: 127.0.0.1:6379, password mychat-dev-pass
MSG
}

ensure_redis_dependency

echo "==> Configuring default no-ODB/no-gRPC regression build: ${build_dir}"
cmake -S . -B "${build_dir}" \
  -DVCPKG_INSTALLED_DIR="${vcpkg_installed_dir}" \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DCMAKE_BUILD_TYPE="${build_type}"

regenerate_common_proto

echo "==> Building default regression targets"
cmake --build "${build_dir}" -j"${jobs}"

echo "==> Running default regression tests"
ctest --test-dir "${build_dir}" --output-on-failure

echo "==> Default regression CI passed"
