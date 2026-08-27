#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

resolve_compose_cmd() {
  if docker compose version >/dev/null 2>&1; then
    COMPOSE_CMD=(docker compose)
    return
  fi

  cat >&2 <<'EOF'
Error: Docker Compose v2 is required on this machine.

Install Docker Compose plugin and re-run this script:
  sudo apt-get install -y docker-compose-plugin

Then verify with:
  docker compose version
EOF
  exit 1
}

resolve_compose_cmd

"${COMPOSE_CMD[@]}" up -d qt-dev

echo "qt-dev container is running."
