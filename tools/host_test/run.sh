#!/usr/bin/env bash
# Compile and run the host-side tests for the firmware's pure logic.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="$(mktemp -d)"
CXX="${CXX:-g++}"

if ! command -v "$CXX" >/dev/null 2>&1; then
  echo "no C++ compiler found (set CXX)" >&2
  exit 1
fi

# $1 = test source, remaining args = firmware sources it needs
run_test() {
  local name="$1"; shift
  "$CXX" -std=c++17 -Wall -Wextra -O1 \
    -I "$HERE/shim" \
    -I "$ROOT/firmware/include" \
    "$HERE/$name" "$HERE/shim/gfx_tracking.cpp" "$@" \
    -o "$OUT/$name.bin"
  "$OUT/$name.bin"
}

run_test test_text.cpp  "$ROOT/firmware/src/text.cpp"
run_test test_faces.cpp "$ROOT/firmware/src/faces.cpp"
