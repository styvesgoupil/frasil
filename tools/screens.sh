#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# FRASIL's screenshots, from the host simulator (app/tests/screens.txt): the screen's own pixels for the manual
# (app/docs/img, 240 x 240), and three times the size, pixel for pixel, for the README (docs/img).
#   tools/screens.sh            (after ./build.sh --host)
#   tools/screens.sh smk37      the SMK-37's (card #111): screens.txt, then app/tests/screens_smk37.txt (what only it
#                               has), on its simulator, into app/docs/img/smk37 (the site's guide and the books take
#                               them there; no README copy). After FM1B_BOARD=smk37 FM1B_MODEL=elite ./build.sh --host
set -e
cd "$(dirname "$0")/.."
PY="${PYTHON:-.venv/bin/python}"
[ -x "$PY" ] || PY=python3
if [ "${1:-}" = smk37 ]; then
    H=app/build/host/frasil-smk37-elite_host
    [ -x "$H" ] || { echo "screens.sh: no SMK-37 simulator (FM1B_BOARD=smk37 FM1B_MODEL=elite ./build.sh --host)"; exit 1; }
    OUT="$(mktemp -d)"
    trap 'rm -rf "$OUT"' EXIT
    "$H" app/tests/screens.txt "$OUT" >/dev/null
    "$H" app/tests/screens_smk37.txt "$OUT" >/dev/null
    mkdir -p app/docs/img/smk37
    rm -f app/docs/img/smk37/screen-*.png
    cp "$OUT"/screen-*.png app/docs/img/smk37/
    exec "$PY" - <<'EOF'
from pathlib import Path
from PIL import Image
fs = sorted(Path("app/docs/img/smk37").glob("screen-*.png"))
for f in fs:
    Image.open(f).save(f, optimize=True)      # the simulator's PNGs are stored, not compressed
print(f"screens  app/docs/img/smk37  {len(fs)} screens")
EOF
fi
[ -x app/build/host/frasil_host ] || { echo "screens.sh: no host simulator (./build.sh --host)"; exit 1; }
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
app/build/host/frasil_host app/tests/screens.txt "$OUT" >/dev/null
mkdir -p app/docs/img docs/img
rm -f app/docs/img/screen-*.png docs/img/screen-*.png
cp "$OUT"/screen-*.png app/docs/img/
"$PY" - <<'EOF'
from pathlib import Path
from PIL import Image
for f in sorted(Path("app/docs/img").glob("screen-*.png")):
    im = Image.open(f)
    im.save(f, optimize=True)                 # the simulator's PNGs are stored, not compressed
    im.resize((im.width * 3, im.height * 3), Image.NEAREST).save(Path("docs/img") / f.name, optimize=True)
    print(f.name)
EOF
