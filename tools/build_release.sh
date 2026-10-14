#!/usr/bin/env bash
# ---------------------------------------------------------------------------
#  Build the firmware, build the LittleFS web panel, then merge everything into
#  a single .bin that ESP Web Tools can flash in one click.
#
#  Usage:  ./tools/build_release.sh
# ---------------------------------------------------------------------------
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PLATFORMIO_CORE_DIR="$ROOT/.pio-core"

PIO="$ROOT/.venv-pio/bin/pio"
PY="$ROOT/.venv-pio/bin/python"

if [ ! -x "$PIO" ]; then
  echo "PlatformIO is not installed yet. Run:" >&2
  echo "  python3 -m venv .venv-pio && ./.venv-pio/bin/python -m pip install platformio esptool" >&2
  exit 1
fi

cd "$ROOT/firmware"

echo "==> 1/3 compiling firmware"
"$PIO" run

echo
echo "==> 2/3 building LittleFS image (the on-device web panel)"
"$PIO" run -t buildfs

echo
echo "==> 3/3 merging into one image"
"$PY" "$ROOT/tools/merge_firmware.py"

echo
echo "Done."
echo "  * flash over USB:      $PIO run -t upload && $PIO run -t uploadfs"
echo "  * serve the installer: cd web-flasher && python3 -m http.server 8000"
echo "                         then open http://localhost:8000"
