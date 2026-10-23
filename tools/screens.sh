#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# FRASIL's screenshots, from the host simulator (app/tests/screens.txt): the screen's own pixels for the manual
# (app/docs/img, 240 x 240), and three times the size, pixel for pixel, for the README (docs/img).
#   tools/screens.sh            (after ./build.sh --host)
set -e
cd "$(dirname "$0")/.."
PY="${PYTHON:-.venv/bin/python}"
[ -x "$PY" ] || PY=python3
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
