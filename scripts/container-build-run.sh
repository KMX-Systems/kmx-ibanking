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

# Allow the container's root user to connect to your local X server.
xhost +si:localuser:root >/dev/null

cleanup() {
  xhost -si:localuser:root >/dev/null
}
trap cleanup EXIT

"${COMPOSE_CMD[@]}" up -d qt-dev
"${COMPOSE_CMD[@]}" exec qt-dev bash -lc '/workspace/scripts/in-container/configure-build.sh'

# Arguments to this script reach the app, e.g. --ui=mobile --device=budget.
remote_cmd='export LIBGL_DRI3_DISABLE=${LIBGL_DRI3_DISABLE:-1}'
if [[ "${USE_SOFTWARE_RENDERING:-0}" == "1" ]]; then
  remote_cmd+=' LIBGL_ALWAYS_SOFTWARE=1 QT_QUICK_BACKEND=software QSG_RHI_BACKEND=software'
fi
remote_cmd+='; /workspace/build/appkmxbank'
if (( $# )); then
  remote_cmd+=" $(printf '%q ' "$@")"
fi

"${COMPOSE_CMD[@]}" exec qt-dev bash -lc "$remote_cmd"
