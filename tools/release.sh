#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# A FRASIL release (RELEASING.md).
#   tools/release.sh             the checks, then the package app/build/frasil-VERSION.fwsc (VERSION's), verified
#   tools/release.sh --publish   and then the tag vVERSION, the GitHub release with the package, and the site
# The FM-1 Firmware Hub takes the newest GitHub release by itself: --publish is what puts a version in players' hands.
set -e
cd "$(dirname "$0")/.."
V="$(tr -d ' \n' < VERSION)"
PUBLISH="${1:-}"
PY="${PYTHON:-.venv/bin/python}"
[ -x "$PY" ] || PY=python3
PKG="app/build/frasil-$V.fwsc"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"

# the version in CHANGELOG.md, and a tree with nothing uncommitted (the release is the source at its tag)
grep -q "^## $V " CHANGELOG.md || grep -q "^## $V\$" CHANGELOG.md || { echo "release: no '## $V' section in CHANGELOG.md"; exit 1; }
if [ -n "$(git status --porcelain)" ]; then
    echo "release: uncommitted changes (commit them first: the release is the source at its tag)"
    [ "$PUBLISH" = "--publish" ] && exit 1
fi

# the host checks
./build.sh --host >/dev/null
OUT="$(app/build/host/frasil_host app/tests/basic.txt app/build/host)"
OK="$(printf '%s\n' "$OUT" | grep -c '^  ok' || true)"
if printf '%s\n' "$OUT" | grep -q '^FAIL'; then
    printf '%s\n' "$OUT" | grep '^FAIL'
    echo "release: host checks failed"
    exit 1
fi
echo "checks   $OK ok"

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

if [ "$PUBLISH" != "--publish" ]; then
    echo "ready: try $PKG on the FM-1 (python3 base/tools/fm1_install.py $PKG), then tools/release.sh $V --publish"
    exit 0
fi

# the tag, the release (a full one, not a pre-release: the hub and GitHub's "latest" take full releases; the hub
# marks versions below 1.0 beta by itself), the notes from CHANGELOG.md
NOTES="$TMP/notes.md"
awk -v v="$V" '$0 ~ "^## " v "( |$)" {on = 1; next} /^## / {on = 0} on' CHANGELOG.md > "$NOTES"
git tag -a "v$V" -m "FRASIL $V"
git push origin "v$V"
gh release create "v$V" "$PKG" --title "FRASIL $V" --notes-file "$NOTES"
# the site: front page, installer, emulator, manual (GitHub Pages, the gh-pages branch)
PYTHON="$PY" base/tools/publish_site.sh app "$V"
