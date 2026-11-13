#!/bin/sh
# Build and run the KamoteKeys host tests (Linux/macOS, any C11 compiler).
# These verify logic and render LCD screenshots; they do NOT replace the
# JieLi pi32v2 firmware build.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${OUT:-$ROOT/build/host}
mkdir -p "$OUT"
CC=${CC:-cc}
$CC -std=c11 -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
  -DFM1_HOST_TEST -fsanitize=address,undefined \
  -I"$ROOT/sdk/include" -I"$ROOT/app/synth" -I"$ROOT/app" -I"$ROOT/app/board" -I"$ROOT/app/kamotkeys" -I"$ROOT/tests/host" \
  "$ROOT"/app/kamotkeys/*.c "$ROOT/app/screen.c" "$ROOT/app/smk37_controls.c" "$ROOT/app/midi_router.c" "$ROOT/tests/host/stubs.c" "$ROOT/tests/host/test_main.c" \
  -o "$OUT/kk_tests"
cd "$OUT" && ./kk_tests "$@"
