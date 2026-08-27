#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="${PROJECT_ROOT:-/workspace}"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"
APP_PATH="${APP_PATH:-$BUILD_DIR/appkmxbank}"

if [[ ! -x "$APP_PATH" ]]; then
  echo "App binary not found or not executable at: $APP_PATH" >&2
  echo "Run scripts/in-container/configure-build.sh first." >&2
  exit 1
fi

if [[ -z "${DISPLAY:-}" ]]; then
  echo "DISPLAY is not set inside container." >&2
  echo "Enter via scripts/container-enter.sh from host (it sets X11 permissions)." >&2
  exit 1
fi

export LIBGL_DRI3_DISABLE="${LIBGL_DRI3_DISABLE:-1}"

if [[ "${USE_SOFTWARE_RENDERING:-0}" == "1" ]]; then
  export LIBGL_ALWAYS_SOFTWARE=1
  export QT_QUICK_BACKEND=software
  export QSG_RHI_BACKEND=software
fi

# Any extra arguments go straight to the app, e.g. --ui=mobile --device=budget
# (run with --list-devices to see the mobile presets).
# `|| status=$?` keeps set -e from aborting before the hint below can print.
status=0
"$APP_PATH" "$@" || status=$?

# Qt reports a missing X11 grant as "could not load the Qt platform plugin xcb",
# which reads like a broken install. It usually just means the host revoked the
# xhost grant this container connects with.
if [[ $status -ne 0 ]]; then
  cat >&2 <<EOF

If the failure above was "could not connect to display $DISPLAY", run this on
the HOST (not in the container) and try again:
  xhost +si:localuser:root

scripts/container-enter.sh does that for you, but it revokes the grant again
when that shell exits.
EOF
fi

exit $status
