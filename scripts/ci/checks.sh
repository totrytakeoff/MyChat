#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${repo_root}"

echo "==> Checking JSON agent state"
python -m json.tool docs/agent_context/state.json >/dev/null

echo "==> Checking whitespace in latest commit"
git diff-tree --check --root --no-commit-id -r HEAD

echo "==> Checking whitespace in working tree diff"
git diff --check

echo "==> CI checks passed"
