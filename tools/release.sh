#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# A FRASIL release (RELEASING.md).
#   tools/release.sh             the checks, then the package app/build/frasil-VERSION.fwsc (VERSION's), verified
#   RELEASE_NOTES_LATER=1 tools/release.sh   the same before CHANGELOG.md has its section (the runner's PREPARE:
#                                DELIVER writes it from the RELEASE NOTES)
#   tools/release.sh --publish   refuses: this repository's history is private. The public repository gets one snapshot
#                                commit of the firmware only (tools/public_snapshot.sh), tagged there (RELEASING.md).
# The FM-1 Firmware Hub takes the newest GitHub release of the public repository by itself.
set -e
cd "$(dirname "$0")/.."
V="$(tr -d ' \n' < VERSION)"
PUBLISH="${1:-}"
# Tagging HEAD and pushing the tag to origin would publish this repository's whole history (the websites, the River).
if [ "$PUBLISH" = "--publish" ]; then
    echo "release: --publish is gone. The public repository (origin) holds firmware-only snapshots, never this history."
    echo "  tools/public_snapshot.sh dev --commit \"FRASIL $(tr -d ' \n' < "$(dirname "$0")/../VERSION")\"   then the steps in RELEASING.md (Publishing)"
    exit 1
fi
PY="${PYTHON:-.venv/bin/python}"
[ -x "$PY" ] || PY=python3
PKG="app/build/frasil-$V.fwsc"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"

# the version in CHANGELOG.md, and a tree with nothing uncommitted (the release is the source at its tag).
# RELEASE_NOTES_LATER=1 (the runner's PREPARE): the section is DELIVER's, written from the RELEASE NOTES: a word, not a stop
if ! grep -q "^## $V " CHANGELOG.md && ! grep -q "^## $V\$" CHANGELOG.md; then
    if [ "${RELEASE_NOTES_LATER:-}" = 1 ]; then
        echo "release: no '## $V' section in CHANGELOG.md yet (DELIVER writes it from the RELEASE NOTES)"
    else
        echo "release: no '## $V' section in CHANGELOG.md"; exit 1
    fi
fi
if [ -n "$(git status --porcelain)" ]; then
    echo "release: uncommitted changes (commit them first: the release is the source at its tag)"
    [ "$PUBLISH" = "--publish" ] && exit 1
fi

# the host checks
./build.sh --host >/dev/null
OUT="$(app/build/host/frasil_host app/tests/basic.txt app/build/host; app/build/host/frasil_host app/tests/boot.txt app/build/host; app/build/host/frasil_host app/tests/ux.txt app/build/host)"
OK="$(printf '%s\n' "$OUT" | grep -c '^  ok' || true)"
if printf '%s\n' "$OUT" | grep -q '^FAIL'; then
    printf '%s\n' "$OUT" | grep '^FAIL'
    echo "release: host checks failed"
    exit 1
fi
echo "checks   $OK ok"

# the engine alone (engine/tests/run.sh: its API, at 48 and 96 kHz, the simulator's renders bit for bit, FRS_VERSION
# VERSION's)
sh engine/tests/run.sh "$TMP/engine" > "$TMP/engine.log" 2>&1 || { cat "$TMP/engine.log"; echo "release: the engine's checks failed"; exit 1; }
echo "engine   $(grep -h -c '^  ok' "$TMP/engine/api.txt") API ok, the simulator's renders bit for bit"

# FRASIL STREAM: this version's engine kept (website/stream/engines/: a stream plays on the engine it was made on, for ever).
# Missing: a word now, a stop at --publish (tools/stream_wasm.sh --keep, then commit it)
if [ ! -f "website/stream/engines/$V.wasm" ]; then
    echo "release: website/stream/engines/$V.wasm is not kept (tools/stream_wasm.sh --keep, then commit it)"
    [ "$PUBLISH" = "--publish" ] && exit 1
fi

# the package, and what the hub checks: an FM-1 identity of FRASIL's own (FM-1_47XXYYZZ), the update loader
./build.sh --release "$V" > "$TMP/build.log" || { cat "$TMP/build.log"; exit 1; }
tail -1 "$TMP/build.log"
"$PY" - "$PKG" "$V" <<'EOF'
import hashlib, re, sys
sys.path.insert(0, "base/tools")
from make_site import product_of
pkg, v = sys.argv[1], sys.argv[2]
raw = open(pkg, "rb").read()
m = re.fullmatch(r"(\d{1,2})\.(\d{1,2})(?:\.(\d{1,2}))?(-[A-Za-z0-9]+)?", v)
want = "FM-1_47%02d%02d%02d" % (int(m[1]), int(m[2]), int(m[3] or 0))
got = product_of(raw)
if got != want:
    raise SystemExit(f"release: {pkg} says {got}, not {want}")
print(f"package  {pkg}  {len(raw)} B  {got}  sha256 {hashlib.sha256(raw).hexdigest()}")
EOF

echo "ready: try $PKG on the FM-1 (python3 base/tools/fm1_install.py $PKG), then publish the snapshot (RELEASING.md)"
