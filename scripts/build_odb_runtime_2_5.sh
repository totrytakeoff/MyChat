#!/usr/bin/env bash
#
# build_odb_runtime_2_5.sh
#
# Build and install ODB 2.5.0 runtime libraries (libodb + libodb-pgsql)
# from source. This ensures the runtime matches the ODB compiler version
# (2.5.0) since vcpkg only provides 2.4.0.
#
# Prerequisites:
#   - build2 toolchain (bdep, b) — https://build2.org/
#   - g++ / libstdc++ (C++20 capable)
#   - libpq headers (libpq-fe.h) — from vcpkg feature pgsql-odb or system pkg
#   - curl, tar
#
# Usage:
#   ./scripts/build_odb_runtime_2_5.sh [options]
#
# Options:
#   --prefix PATH       Install prefix (default: .odb/installed)
#   --src-dir PATH      Source tarball extraction dir (default: .odb/src)
#   --pg-include-dir PATH  Path containing libpq-fe.h (auto-detect)
#   --clean             Remove build artifacts after install
#   --help              Show this message

set -euo pipefail

ODB_MAJOR=2
ODB_MINOR=5
ODB_PATCH=0
ODB_VERSION="${ODB_MAJOR}.${ODB_MINOR}.${ODB_PATCH}"
ODB_VERSION_NUM=20500     # ODB_VERSION value in version.hxx

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Defaults relative to project root
PREFIX="${PROJECT_ROOT}/.odb/installed"
SRC_DIR="${PROJECT_ROOT}/.odb/src"
CLEAN=false

# URL base for ODB source tarballs
ODB_URL_BASE="https://www.codesynthesis.com/download/odb/${ODB_VERSION}"

# Manual compile jobs
JOBS=$(nproc 2>/dev/null || echo 2)

# ------------------------------------------------------------------
# Parse arguments
# ------------------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --prefix) PREFIX="$2"; shift 2 ;;
        --src-dir) SRC_DIR="$2"; shift 2 ;;
        --pg-include-dir) PG_INCLUDE_DIR="$2"; shift 2 ;;
        --clean) CLEAN=true; shift ;;
        --help)
            sed -n '3,20p' "$0"
            exit 0
            ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

canonical_dir() {
    local path="$1"
    mkdir -p "$path"
    cd "$path" && pwd
}

PREFIX="$(canonical_dir "$PREFIX")"
SRC_DIR="$(canonical_dir "$SRC_DIR")"

# ------------------------------------------------------------------
# Find PostgreSQL include path (needed for libodb-pgsql)
# ------------------------------------------------------------------
find_pg_include() {
    # Common locations for libpq-fe.h
    for dir in \
        "${PG_INCLUDE_DIR-}" \
        "${VCPKG_ROOT:-}/installed/x64-linux/include" \
        "${HOME}/pkgs/vcpkg/installed/x64-linux/include" \
        "/usr/include" \
        "/usr/include/postgresql" \
        "/usr/local/include" \
        "/opt/homebrew/include" \
        ; do
        if [ -f "$dir/libpq-fe.h" ]; then
            echo "$dir"
            return 0
        fi
    done
    return 1
}

PG_INCLUDE=$(find_pg_include) || {
    echo "ERROR: libpq-fe.h not found."
    echo "Set --pg-include-dir or set VCPKG_ROOT to the vcpkg root."
    exit 1
}
echo "Found libpq-fe.h in: ${PG_INCLUDE}"

# ------------------------------------------------------------------
# Check prerequisites
# ------------------------------------------------------------------
check_prereqs() {
    local missing=false
    for cmd in bdep b curl tar g++; do
        if ! command -v "$cmd" &>/dev/null; then
            echo "ERROR: Required command '$cmd' not found."
            missing=true
        fi
    done
    if "$missing"; then
        echo "Install missing prerequisites and try again."
        exit 1
    fi
    # Verify build2 works
    if ! b --version >/dev/null 2>&1; then
        echo "ERROR: 'b' (build2) does not work. Check build2 installation."
        exit 1
    fi
}

# ------------------------------------------------------------------
# Download ODB source tarballs
# ------------------------------------------------------------------
download_sources() {
    mkdir -p "$SRC_DIR"
    cd "$SRC_DIR"

    for pkg in libodb libodb-pgsql; do
        local tarball="${pkg}-${ODB_VERSION}.tar.gz"
        local dir="${pkg}-${ODB_VERSION}"
        if [ -d "$dir" ]; then
            echo "Source directory ${dir} already exists, skipping download."
        else
            if [ -f "$tarball" ]; then
                echo "Tarball ${tarball} exists, extracting..."
            else
                local url="${ODB_URL_BASE}/${tarball}"
                echo "Downloading ${url} ..."
                curl -fSL -o "$tarball" "$url" || {
                    echo "Failed to download ${url}"
                    exit 1
                }
            fi
            tar xzf "$tarball"
            echo "Extracted ${tarball}"
        fi
    done
}

# ------------------------------------------------------------------
# Build libodb with build2
# ------------------------------------------------------------------
build_libodb() {
    echo ""
    echo "=== Building libodb ${ODB_VERSION} ==="

    local src="${SRC_DIR}/libodb-${ODB_VERSION}"
    local build_dir="${SRC_DIR}/build-libodb"
    local install_manifest="${PREFIX}/lib/libodb.a"

    if [ -f "$install_manifest" ]; then
        echo "libodb.a already installed at ${install_manifest}, skipping."
        return 0
    fi

    # Clean any previous build2 config state in the project
    rm -rf "${src}/.bdep" "$build_dir"

    cd "$src"
    echo "Initializing build2 configuration..."
    bdep init -C "$build_dir" -- cc \
        config.cxx=g++ \
        config.cxx.coptions="-O2" \
        config.install.root="$PREFIX" \
        config.install.lib="$PREFIX/lib" \
        config.install.include="$PREFIX/include"

    cd "$build_dir"
    echo "Building libodb..."
    b
    echo "Installing libodb to ${PREFIX}..."
    b install
    echo "libodb build complete."
}

# ------------------------------------------------------------------
# Build libodb-pgsql manually
# ------------------------------------------------------------------
build_libodb_pgsql() {
    echo ""
    echo "=== Building libodb-pgsql ${ODB_VERSION} ==="

    local src="${SRC_DIR}/libodb-pgsql-${ODB_VERSION}"
    local build_dir="${SRC_DIR}/build-libodb-pgsql-manual"
    local obj_dir="${build_dir}/obj"
    local install_manifest="${PREFIX}/lib/libodb-pgsql.a"

    if [ -f "$install_manifest" ]; then
        echo "libodb-pgsql.a already installed at ${install_manifest}, skipping."
        return 0
    fi

    mkdir -p "$obj_dir"

    # Install headers FIRST so compiled .cxx files find the right includes.
    # Copy all headers from source tree to install prefix.
    echo "Installing libodb-pgsql headers..."
    mkdir -p "${PREFIX}/include/odb/pgsql/details"

    # Copy all header types (hxx, ixx, txx)
    cp -r "${src}/odb/pgsql/"*.hxx "${PREFIX}/include/odb/pgsql/" 2>/dev/null || true
    cp -r "${src}/odb/pgsql/"*.ixx "${PREFIX}/include/odb/pgsql/" 2>/dev/null || true
    cp -r "${src}/odb/pgsql/"*.txx "${PREFIX}/include/odb/pgsql/" 2>/dev/null || true

    # Copy details headers (including txx template implementations)
    if [ -d "${src}/odb/pgsql/details" ]; then
        cp -r "${src}/odb/pgsql/details/"*.hxx "${PREFIX}/include/odb/pgsql/details/" 2>/dev/null || true
        cp -r "${src}/odb/pgsql/details/"*.ixx "${PREFIX}/include/odb/pgsql/details/" 2>/dev/null || true
        cp -r "${src}/odb/pgsql/details/"*.txx "${PREFIX}/include/odb/pgsql/details/" 2>/dev/null || true
    fi

    # Copy pregenerated details headers (options.hxx, options.ixx)
    local predir="${src}/odb/pgsql/details/pregenerated"
    if [ -d "$predir" ]; then
        find "$predir" -name "*.hxx" -o -name "*.ixx" -o -name "*.txx" | while IFS= read -r f; do
            local rel="${f#$predir/}"
            local target="${PREFIX}/include/${rel}"
            mkdir -p "$(dirname "$target")"
            cp "$f" "$target"
        done
    fi

    # Collect all .cxx source files (excluding tests/)
    local sources
    sources=$(find "$src" -name "*.cxx" -not -path "*/tests/*" | sort)

    # Include paths (order matters):
    #   1. installed pgsql headers (from copy above — correct 2.5.0)
    #   2. installed libodb core headers
    #   3. PostgreSQL headers (libpq-fe.h) — narrow, not vcpkg root
    # Crucially, we do NOT add the libodb-pgsql source tree root, because
    # its pregenerated/ directory would conflict with the installed headers.
    local cxxflags="-std=c++20 -O2 -fPIC"
    local includes="-I${PREFIX}/include -I${PG_INCLUDE}"

    echo "Compiling ${ODB_VERSION} source files..."
    local obj_files=""
    local count=0 total
    total=$(echo "$sources" | wc -l)

    for src_file in $sources; do
        local rel="${src_file#$src/}"
        rel="${rel//\//_}"
        local obj_file="${obj_dir}/${rel}.o"
        obj_files="$obj_files $obj_file"

        if [ ! -f "$obj_file" ]; then
            echo "  [${count}/${total}] ${rel}"
            g++ $cxxflags $includes -c "$src_file" -o "$obj_file"
        else
            echo "  [SKIP] ${rel} (cached)"
        fi
        count=$((count + 1))
    done

    echo "Archiving libodb-pgsql.a..."
    mkdir -p "${PREFIX}/lib"
    ar rcs "${PREFIX}/lib/libodb-pgsql.a" $obj_files

    echo "libodb-pgsql build complete."
}

# ------------------------------------------------------------------
# Verify installation
# ------------------------------------------------------------------
verify() {
    echo ""
    echo "=== Verification ==="

    local errors=0

    # Check version.hxx exists and has correct version
    local vh="${PREFIX}/include/odb/version.hxx"
    if [ -f "$vh" ]; then
        if grep -q "#define ODB_VERSION[[:space:]]*${ODB_VERSION_NUM}" "$vh"; then
            echo "OK: version.hxx reports ODB_VERSION ${ODB_VERSION_NUM} (${ODB_VERSION})"
        else
            echo "ERROR: version.hxx has unexpected version"
            grep "#define ODB_VERSION" "$vh"
            errors=$((errors + 1))
        fi
    else
        echo "ERROR: ${vh} not found"
        errors=$((errors + 1))
    fi

    # Check library files
    for lib in libodb.a libodb-pgsql.a; do
        if [ -f "${PREFIX}/lib/${lib}" ]; then
            local size
            size=$(stat --printf="%s" "${PREFIX}/lib/${lib}" 2>/dev/null || stat -f"%z" "${PREFIX}/lib/${lib}" 2>/dev/null)
            echo "OK: ${lib} (${size} bytes)"
        else
            echo "ERROR: ${PREFIX}/lib/${lib} not found"
            errors=$((errors + 1))
        fi
    done

    if [ "$errors" -gt 0 ]; then
        echo "FAILED: ${errors} error(s) found."
        exit 1
    fi
    echo "SUCCESS: ODB ${ODB_VERSION} runtime installed at ${PREFIX}"
}

# ------------------------------------------------------------------
# Clean build artifacts (optional)
# ------------------------------------------------------------------
cleanup() {
    if "$CLEAN"; then
        echo ""
        echo "Cleaning build artifacts..."
        rm -rf "${SRC_DIR}/build-libodb" "${SRC_DIR}/build-libodb-pgsql-manual"
        rm -rf "${SRC_DIR}/libodb-${ODB_VERSION}/.bdep"
        echo "Done."
    fi
}

# ------------------------------------------------------------------
# Main
# ------------------------------------------------------------------
echo "ODB ${ODB_VERSION} Runtime Build Script"
echo "  Prefix: ${PREFIX}"
echo "  Source: ${SRC_DIR}"
echo "  libpq:  ${PG_INCLUDE}"
echo ""

check_prereqs
download_sources
build_libodb
build_libodb_pgsql
verify
cleanup

echo ""
echo "To use this runtime, configure CMake with:"
echo "  cmake ... -DMYCHAT_ODB_ROOT=\"${PREFIX}\" ..."
echo "or let CMake auto-detect it at the default .odb/installed location."
