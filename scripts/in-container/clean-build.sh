#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="${PROJECT_ROOT:-/workspace}"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"

if [[ -d "$BUILD_DIR" ]]; then
  rm -rf "$BUILD_DIR"
  echo "Removed build directory: $BUILD_DIR"
else
  echo "Build directory does not exist: $BUILD_DIR"
fi
