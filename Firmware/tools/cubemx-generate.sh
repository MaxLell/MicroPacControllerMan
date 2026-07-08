#!/usr/bin/env bash
# Regenerate STM32CubeMX init code from pacman.ioc, headless.
# Requires STM32CubeMX installed. Point to it via $STM32CUBEMX or have it on PATH.
set -euo pipefail

HERE="$(cd "$(dirname "$0")/.." && pwd)"
IOC="$HERE/pacman.ioc"

CUBEMX="${STM32CUBEMX:-$(command -v STM32CubeMX || true)}"
if [ -z "$CUBEMX" ]; then
  echo "ERROR: STM32CubeMX not found." >&2
  echo "  Install it, then either add it to PATH or run:" >&2
  echo "    export STM32CUBEMX=/path/to/STM32CubeMX/STM32CubeMX" >&2
  exit 1
fi
if [ ! -f "$IOC" ]; then
  echo "ERROR: $IOC not found." >&2
  exit 1
fi

SCRIPT="$(mktemp)"
trap 'rm -f "$SCRIPT"' EXIT
cat > "$SCRIPT" <<EOF
config load $IOC
project generate
exit
EOF

echo "Running CubeMX headless generation from $IOC ..."
"$CUBEMX" -q "$SCRIPT"
echo "Done. Generated CMake project under $HERE"
