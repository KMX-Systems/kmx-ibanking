#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

# Allow container root user to access host X server for GUI apps.
xhost +si:localuser:root >/dev/null
cleanup() {
  xhost -si:localuser:root >/dev/null
}
trap cleanup EXIT

docker compose up -d qt-dev >/dev/null

CONTAINER_ID="$(docker compose ps -q qt-dev)"
if [[ -z "$CONTAINER_ID" ]]; then
  echo "Could not resolve qt-dev container ID. Check docker compose status." >&2
  exit 1
fi

if [[ -t 0 && -t 1 ]]; then
  docker exec -it "$CONTAINER_ID" bash
else
  docker exec "$CONTAINER_ID" bash
fi
