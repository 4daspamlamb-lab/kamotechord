#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
# Run every KamoteKeys test for every device (Linux/macOS).
#   WASI_SDK=/path/to/wasi-sdk-27 tools/test-all.sh
# Without WASI_SDK the browser-simulator tests are skipped.
set -e
TOP=$(cd "$(dirname "$0")/.." && pwd)
OUT=$TOP/build/test; mkdir -p "$OUT"
"$TOP/tools/sync-core.sh" all
echo "== SMK-37 host tests"; OUT=$OUT "$TOP/tests/host/run.sh" | tail -1
echo "== Ableton Remote Script"; python3 "$TOP/tests/remote_script/test_kamotkeys_script.py" 2>&1 | tail -1
if [ -n "$WASI_SDK" ]; then
    for dev in smk37 fm1; do
        cd "$TOP/devices/$dev"
        FILES=$(find app web third_party/synth -type f -name '*.c' ! -path 'app/board/*' | LC_ALL=C sort)
        "$WASI_SDK/bin/clang" --sysroot="$WASI_SDK/share/wasi-sysroot" -std=c11 -O2 -fwrapv -fno-fast-math -ffp-contract=off \
          -DFM1_HOST_TEST -DFM1_OPEN_WEB -Wall -Wextra -Werror -Wno-unused-function -Wno-array-parameter -mexec-model=reactor \
          -Iweb/engine -Iweb/synth -Ithird_party/synth -Isdk/include -Iapp/synth -Iapp $FILES \
          -Wl,--strip-all -Wl,-z,stack-size=131072 -Wl,--initial-memory=4194304 -Wl,--max-memory=33554432 -o "$OUT/$dev.wasm"
    done
    cd "$TOP"
    echo "== SMK-37 simulator"; node tests/integration/sim_test.mjs "$OUT/smk37.wasm" "$OUT" 2>&1 | tail -1
    echo "== FM-1 simulator"; node tests/integration/fm1_sim_test.mjs "$OUT/fm1.wasm" "$OUT" 2>&1 | tail -1
else
    echo "(WASI_SDK not set: simulator tests skipped)"
fi
