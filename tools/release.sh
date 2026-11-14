#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
# Make a KamoteKeys release zip.
#   tools/release.sh 0.60        set the version, test, package
#   tools/release.sh             package the current version
# Steps: set KAMOTKEYS_VERSION in core/kamotkeys/kk_config.h, run
# tools/test-all.sh (stops on any failure), then write
# build/KamoteKeys-<v>.zip (Remote Script, README, CHANGELOG, docs, and a
# git-archive source zip of HEAD, so commit first) and
# build/KamoteKeys-<v>/BUILD.txt (the PowerShell build blocks), printed at the end.
set -e
TOP=$(cd "$(dirname "$0")/.." && pwd); cd "$TOP"
CFG=core/kamotkeys/kk_config.h
if [ -n "$1" ]; then
    sed -i.bak "s/^#define KAMOTKEYS_VERSION \"[^\"]*\"/#define KAMOTKEYS_VERSION \"$1\"/" "$CFG" && rm -f "$CFG.bak"
fi
V=$(sed -n 's/^#define KAMOTKEYS_VERSION "\([^"]*\)".*/\1/p' "$CFG")
[ -n "$V" ] || { echo "no KAMOTKEYS_VERSION in $CFG" >&2; exit 1; }
grep -q "^## KamoteKeys $V" CHANGELOG.md || echo "warning: CHANGELOG.md has no '## KamoteKeys $V' section" >&2
[ -z "$WASI_SDK" ] && [ -d /tmp/claude-0/wasi/wasi-sdk-27.0-x86_64-linux ] && export WASI_SDK=/tmp/claude-0/wasi/wasi-sdk-27.0-x86_64-linux
if [ "$SKIP_TESTS" != 1 ]; then
    tools/test-all.sh | tee build/test-all.log
    if grep -E "[1-9][0-9]* failures|FAIL|Error" build/test-all.log >/dev/null; then echo "tests failed" >&2; exit 1; fi
fi
[ -z "$(git status --porcelain -- core devices remote_script docs README.md CHANGELOG.md)" ] ||
    echo "warning: uncommitted changes; the source zip is HEAD only" >&2
D=build/KamoteKeys-$V
rm -rf "$D" "$D.zip"; mkdir -p "$D/Ableton" "$D/docs"
cp -r remote_script/KamoteKeys "$D/Ableton/"; find "$D/Ableton" -name __pycache__ -exec rm -rf {} +
cp README.md CHANGELOG.md HOW_TO_USE.md "$D/"; cp docs/*.md "$D/docs/"; cp -r docs/screenshots docs/diagrams "$D/docs/"
git archive --format=zip --prefix="kamotekeys-$V/" -o "$D/kamotekeys-$V-source.zip" HEAD
block() { # device, output html (relative to the source folder), name on the Mac
cat <<PS
\$v = '$V'
cd \$env:USERPROFILE\\Desktop
Copy-Item \\\\Mac\\Home\\Downloads\\KamoteKeys-\$v.zip . -Force
Expand-Archive .\\KamoteKeys-\$v.zip . -Force
Expand-Archive .\\KamoteKeys-\$v\\kamotekeys-\$v-source.zip . -Force
cd .\\kamotekeys-\$v
\$env:FM1_PI32_TOOLCHAIN = 'C:\\JL\\pi32'
\$env:FM1_SOURCE_URL = "https://localhost/kamotekeys-\$v-source.zip"
\$env:FM1_WASI_SDK = "\$env:USERPROFILE\\Desktop\\kamotkeys-0.30-smk37_open\\build\\tools\\wasi-sdk-27.0-x86_64-windows"
.\\build.bat $1 --package
Copy-Item $2 \\\\Mac\\Home\\Downloads\\$3-KamoteKeys-\$v.html -Force
PS
}
{
echo "KamoteKeys $V: build in Windows PowerShell (Parallels). Put KamoteKeys-$V.zip in the Mac's Downloads first."
echo "Remove the FM1_WASI_SDK line if that folder does not exist (the build then downloads it)."
echo; echo "## SMK-37 Pro"; block smk37-pro '.\devices\smk37\build\official-web\pro\SMK37-Open-Source.html' SMK37-Pro
echo; echo "## SMK-37 Elite"; block smk37-elite '.\devices\smk37\build\official-web\elite\SMK37-Open-Source.html' SMK37-Elite
echo; echo "## FM-1"; block fm1 '.\devices\fm1\build\official-web\FM1-Official-Open-Source.html' FM1
echo
echo "Flash: open the .html from Downloads in Chrome/Brave on the Mac, connect by USB, allow MIDI + SysEx,"
echo "Device update, keep USB connected until it finishes, power-cycle. Pro file on a Pro, Elite on an Elite."
} > "$D/BUILD.txt"
(cd build && zip -qr "KamoteKeys-$V.zip" "KamoteKeys-$V")
echo "== build/KamoteKeys-$V.zip"; cat "$D/BUILD.txt"
