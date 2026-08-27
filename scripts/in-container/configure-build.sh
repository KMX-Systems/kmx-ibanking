#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="${PROJECT_ROOT:-/workspace}"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"
GENERATOR="${GENERATOR:-Ninja}"

cd "$PROJECT_ROOT"

# The source tree is bind-mounted, so a build directory configured elsewhere
# (on the host, or by an older image) survives into the container and pins the
# compiler it was configured with. Drop such a cache so the image's toolchain
# is the one that gets used.
CACHE="$BUILD_DIR/CMakeCache.txt"
if [[ -n "${CXX:-}" && -f "$CACHE" ]]; then
  cached_cxx="$(sed -n 's/^CMAKE_CXX_COMPILER:[^=]*=//p' "$CACHE")"
  if [[ -n "$cached_cxx" && "$cached_cxx" != "$CXX" ]]; then
    echo "Stale build cache ($cached_cxx != $CXX); reconfiguring from scratch." >&2
    rm -rf "$BUILD_DIR"
  fi
fi

cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -G "$GENERATOR"
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "Build complete: $BUILD_DIR/appkmxbank"
