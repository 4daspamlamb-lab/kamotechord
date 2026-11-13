#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
# Copy the shared KamoteKeys code (core/) into one device tree, or all.
#   tools/sync-core.sh [smk37|fm1|all]
# build.bat does the same on Windows (tools/Sync-Core.ps1) before building.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
sync() {
    dev=$1; app="$ROOT/devices/$dev/app"
    [ -d "$app" ] || { echo "unknown device: $dev" >&2; exit 2; }
    case $dev in smk37) controls=smk37_controls.c ;; *) controls=kk_controls.c ;; esac
    cp "$ROOT"/core/kamotkeys/* "$app/kamotkeys/"
    cp "$ROOT/core/screen.c" "$app/screen.c"
    cp "$ROOT/core/kk_controls.c" "$app/$controls"
}
case ${1:-all} in
    all) sync smk37; sync fm1 ;;
    *) sync "$1" ;;
esac
