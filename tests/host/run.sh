#!/bin/sh
# Build and run the KamoteKeys host tests (Linux/macOS, any C11 compiler)
# against the SMK-37 device tree (the shared core plus SMK-37 glue). These
# verify logic and render LCD screenshots; they do NOT replace the JieLi
# pi32v2 firmware build. The FM-1 is tested in its browser simulator
# (tests/integration/fm1_sim_test.mjs).
set -e
TOP=$(cd "$(dirname "$0")/../.." && pwd)
"$TOP/tools/sync-core.sh" smk37
ROOT=$TOP/devices/smk37
OUT=${OUT:-$TOP/build/host}
mkdir -p "$OUT"
CC=${CC:-cc}
$CC -std=c11 -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
  -DFM1_HOST_TEST -fsanitize=address,undefined \
  -I"$ROOT/sdk/include" -I"$ROOT/app/synth" -I"$ROOT/app" -I"$ROOT/app/board" -I"$ROOT/app/kamotkeys" -I"$TOP/tests/host" \
  "$ROOT"/app/kamotkeys/*.c "$ROOT/app/screen.c" "$ROOT/app/smk37_controls.c" "$ROOT/app/midi_router.c" "$TOP/tests/host/stubs.c" "$TOP/tests/host/test_main.c" \
  -o "$OUT/kk_tests"
cd "$OUT" && ./kk_tests "$@"
