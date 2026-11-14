#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# A FRASIL release (RELEASING.md).
#   tools/release.sh             the checks, then the packages app/build/frasil-VERSION.fwsc (VERSION's, the FM-1's) and
#                                the SMK-37's, a model each: frasil-smk37-elite-VERSION.fwsc (the Elite), frasil-smk37-
#                                pro-VERSION.fwsc (the Pro) and frasil-smk37-mke-VERSION.fwsc (the MKE-P37), verified,
#                                and the FM-1's .rfm1 for the hosts' apps (app/build/frasil-VERSION.rfm1, its test passed)
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

# the packages, and what the hub checks: an identity of FRASIL's own, the update loader. The FM-1's first
# (FM-1_47XXYYZZ), from a tree with no SMK-37 build in it yet, and its .rfm1 (the simulator in WebAssembly for the
# hosts' apps, RemoteFM1: card #163), driven by fm1-base's tests/rfm1_test.mjs; then the SMK-37's (card #111): its
# simulator's scripts (its panel's names, its hands, its MIDI OUT), then a package for each of its three models, the
# same firmware under each one's identity (app/fm1.toml id_prefix_smk37: SMK-37 Elite_4XXYYZZ, SMK-37 Pro_4XXYYZZ,
# MKE-P37_4XXYYZZ; the stock updater takes a model's own alone), which the SMK-37's own page installs (the hub links
# to it). The Elite's first, as it was; the Pro's is fm1-base's frasil-smk37-VERSION.fwsc, named for its model here
./build.sh --release "$V" > "$TMP/build.log" || { cat "$TMP/build.log"; exit 1; }
tail -1 "$TMP/build.log"
RFM1="app/build/frasil-$V.rfm1"
rm -f "$RFM1"
./build.sh --release "$V" --rfm1 > "$TMP/rfm1.log" 2>&1 || { cat "$TMP/rfm1.log"; exit 1; }
[ -f "$RFM1" ] || { tail -5 "$TMP/rfm1.log"; echo "release: no $RFM1"; exit 1; }
if grep -q "Emscripten's LICENSE not found" "$TMP/rfm1.log"; then     # (its runtime is in it: its licence with it)
    echo "release: $RFM1 without Emscripten's licence (no em-config on the PATH)"; exit 1
fi
OUT="$(node base/tests/rfm1_test.mjs "$RFM1" 2>&1)" || { printf '%s\n' "$OUT" | grep -v '^  ok' | tail -5; echo "release: $RFM1 fails its test"; exit 1; }
if printf '%s\n' "$OUT" | grep -q 'FAIL'; then
    printf '%s\n' "$OUT" | grep 'FAIL'; echo "release: $RFM1 fails its test"; exit 1
fi
echo "rfm1     $RFM1  $(wc -c < "$RFM1" | tr -d ' ') B  $(printf '%s\n' "$OUT" | grep -c '^  ok') ok  sha256 $(shasum -a 256 "$RFM1" | cut -d' ' -f1)"
SMK="app/build/frasil-smk37-elite-$V.fwsc"
FM1B_BOARD=smk37 FM1B_MODEL=elite ./build.sh --host > "$TMP/smk-host.log" 2>&1 || { cat "$TMP/smk-host.log"; exit 1; }
for t in smk37_names smk37_hands smk37_midiout; do
    mkdir -p "$TMP/$t"
    OUT="$(app/build/host/frasil-smk37-elite_host "app/tests/$t.txt" "$TMP/$t" 2>&1)" || true
    OK="$(printf '%s\n' "$OUT" | grep -c '^ *ok' || true)"
    if printf '%s\n' "$OUT" | grep -q 'FAIL' || [ "$OK" = 0 ]; then
        printf '%s\n' "$OUT" | grep 'FAIL' || printf '%s\n' "$OUT" | tail -3
        echo "release: the SMK-37's $t failed"
        exit 1
    fi
    echo "smk37    $t: $OK ok"
done
for model in elite pro mke; do
    FM1B_BOARD=smk37 FM1B_MODEL=$model ./build.sh --release "$V" > "$TMP/smk-$model.log" 2>&1 || { cat "$TMP/smk-$model.log"; exit 1; }
    grep '^package' "$TMP/smk-$model.log" | tail -1
done
SMK_PRO="app/build/frasil-smk37-pro-$V.fwsc" SMK_MKE="app/build/frasil-smk37-mke-$V.fwsc"
mv "app/build/frasil-smk37-$V.fwsc" "$SMK_PRO"                      # (its sidecar with it: the same bytes)
mv "app/build/frasil-smk37-$V.fwsc.json" "$SMK_PRO.json" 2>/dev/null || true
"$PY" - "$V" "$PKG" "FM-1_47" "$SMK" "SMK-37 Elite_4" "$SMK_PRO" "SMK-37 Pro_4" "$SMK_MKE" "MKE-P37_4" <<'EOF'
import hashlib, re, sys
sys.path.insert(0, "base/tools")
from make_site import product_of
v, rest = sys.argv[1], sys.argv[2:]
m = re.fullmatch(r"(\d{1,2})\.(\d{1,2})(?:\.(\d{1,2}))?(-[A-Za-z0-9]+)?", v)
for pkg, prefix in zip(rest[::2], rest[1::2]):
    raw = open(pkg, "rb").read()
    want = prefix + "%02d%02d%02d" % (int(m[1]), int(m[2]), int(m[3] or 0))
    got = product_of(raw)
    if got != want:
        raise SystemExit(f"release: {pkg} says {got}, not {want}")
    print(f"package  {pkg}  {len(raw)} B  {got}  sha256 {hashlib.sha256(raw).hexdigest()}")
EOF

echo "ready: try $PKG on the FM-1 (python3 base/tools/fm1_install.py $PKG) and $SMK on an SMK-37 Elite backed up \
(docs/SMK37.md; the Pro's and the MKE-P37's not tested yet), then publish the snapshot (RELEASING.md)"
