#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
MIGRATIONS_DIR="${MYCHAT_MIGRATIONS_DIR:-${ROOT_DIR}/db/migrations}"

DB_HOST="${MYCHAT_PGHOST:-127.0.0.1}"
DB_PORT="${MYCHAT_PGPORT:-5432}"
DB_NAME="${MYCHAT_PGDATABASE:-mychat}"
DB_USER="${MYCHAT_PGUSER:-mychat}"
DB_PASSWORD="${MYCHAT_PGPASSWORD:-mychat-dev-pass}"

if [[ ! -d "${MIGRATIONS_DIR}" ]]; then
  echo "Migration directory does not exist: ${MIGRATIONS_DIR}" >&2
  exit 1
fi

if ! compgen -G "${MIGRATIONS_DIR}"/*.sql >/dev/null; then
  echo "No SQL migrations found in ${MIGRATIONS_DIR}" >&2
  exit 1
fi

run_psql() {
  local sql="$1"
  if command -v psql >/dev/null 2>&1; then
    PGPASSWORD="${DB_PASSWORD}" psql \
      --host="${DB_HOST}" \
      --port="${DB_PORT}" \
      --username="${DB_USER}" \
      --dbname="${DB_NAME}" \
      --set=ON_ERROR_STOP=1 \
      --quiet \
      --tuples-only \
      --no-align \
      --command="${sql}"
    return
  fi

  docker compose exec -T \
    -e PGPASSWORD="${DB_PASSWORD}" \
    postgres \
    psql \
      --username="${DB_USER}" \
      --dbname="${DB_NAME}" \
      --set=ON_ERROR_STOP=1 \
      --quiet \
      --tuples-only \
      --no-align \
      --command="${sql}"
}

run_psql_file() {
  local file="$1"
  if command -v psql >/dev/null 2>&1; then
    PGPASSWORD="${DB_PASSWORD}" psql \
      --host="${DB_HOST}" \
      --port="${DB_PORT}" \
      --username="${DB_USER}" \
      --dbname="${DB_NAME}" \
      --set=ON_ERROR_STOP=1 \
      --file="${file}"
    return
  fi

  docker compose exec -T \
    -e PGPASSWORD="${DB_PASSWORD}" \
    postgres \
    psql \
      --username="${DB_USER}" \
      --dbname="${DB_NAME}" \
      --set=ON_ERROR_STOP=1 \
    < "${file}"
}

sql_escape() {
  printf "%s" "$1" | sed "s/'/''/g"
}

checksum_file() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
    return
  fi
  shasum -a 256 "$1" | awk '{print $1}'
}

echo "==> Ensuring schema_migrations table"
run_psql 'CREATE TABLE IF NOT EXISTS "schema_migrations" (
  "version" TEXT NOT NULL PRIMARY KEY,
  "name" TEXT NOT NULL,
  "checksum" TEXT NOT NULL,
  "applied_at" TIMESTAMPTZ NOT NULL DEFAULT now()
);' >/dev/null

for file in "${MIGRATIONS_DIR}"/*.sql; do
  base="$(basename "${file}")"
  version="${base%%_*}"
  name="${base#*_}"
  name="${name%.sql}"
  checksum="$(checksum_file "${file}")"
  escaped_version="$(sql_escape "${version}")"
  escaped_checksum="$(sql_escape "${checksum}")"

  existing="$(run_psql "SELECT checksum FROM \"schema_migrations\" WHERE version = '${escaped_version}';" | tr -d '[:space:]')"

  if [[ -n "${existing}" ]]; then
    if [[ "${existing}" != "${checksum}" ]]; then
      echo "Checksum mismatch for migration ${base}" >&2
      echo "  recorded: ${existing}" >&2
      echo "  current:  ${checksum}" >&2
      exit 1
    fi
    echo "==> Skipping ${base} (already applied)"
    continue
  fi

  echo "==> Applying ${base}"
  run_psql_file "${file}"
  escaped_name="$(sql_escape "${name}")"
  run_psql "INSERT INTO \"schema_migrations\" (version, name, checksum)
            VALUES ('${escaped_version}', '${escaped_name}', '${escaped_checksum}');" >/dev/null
done

echo "==> PostgreSQL migrations complete"
