#!/usr/bin/env bash
# Compile and run the host-side tests for the firmware's pure logic.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="$(mktemp -d)/host_tests"

CXX="${CXX:-g++}"
if ! command -v "$CXX" >/dev/null 2>&1; then
  echo "no C++ compiler found (set CXX)" >&2
  exit 1
fi

"$CXX" -std=c++17 -Wall -Wextra -O1 \
  -I "$HERE/shim" \
  -I "$ROOT/firmware/include" \
  "$HERE/test_text.cpp" \
  "$ROOT/firmware/src/text.cpp" \
  -o "$OUT"

"$OUT"
