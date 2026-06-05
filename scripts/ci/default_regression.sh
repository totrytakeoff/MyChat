#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${repo_root}"

build_dir="${MYCHAT_CI_DEFAULT_BUILD_DIR:-build/ci-default}"
jobs="${MYCHAT_CI_JOBS:-2}"
build_type="${MYCHAT_CI_BUILD_TYPE:-Debug}"
vcpkg_installed_dir="${MYCHAT_CI_VCPKG_INSTALLED_DIR:-${repo_root}/vcpkg_installed/default}"

echo "==> Configuring default no-ODB/no-gRPC regression build: ${build_dir}"
cmake -S . -B "${build_dir}" \
  -DVCPKG_INSTALLED_DIR="${vcpkg_installed_dir}" \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DCMAKE_BUILD_TYPE="${build_type}"

echo "==> Building default regression targets"
cmake --build "${build_dir}" -j"${jobs}"

echo "==> Running default regression tests"
ctest --test-dir "${build_dir}" --output-on-failure

echo "==> Default regression CI passed"
