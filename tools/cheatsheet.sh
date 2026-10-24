#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# FRASIL's cheat sheet, as files: website/assets/cheatsheet/frasil-cheatsheet.pdf (A4 landscape, one page) and
# frasil-cheatsheet.png (2480 x 1754, A4 at 300 dpi, for The FM-1 Firmware Hub), made by headless Chrome from the
# assembled page (website/pages/cheatsheet.html, website/assets/css/cheatsheet.css): the PDF as it prints, the PNG
# from cheatsheet/#sheet (the sheet alone, the whole window). Run it after changing the sheet; it assembles the site
# before and after (tools/website.sh), so build/website/frasil carries the new files.
#   tools/cheatsheet.sh          CHROME=... another Chrome; PORT=... another port than 8049
set -e
cd "$(dirname "$0")/.."
PY="${PYTHON:-.venv/bin/python}"
[ -x "$PY" ] || PY=python3
CHROME="${CHROME:-/Applications/Google Chrome.app/Contents/MacOS/Google Chrome}"
PORT="${PORT:-8049}"
OUT=website/assets/cheatsheet
[ -x "$CHROME" ] || { echo "cheatsheet.sh: no Chrome at $CHROME (set CHROME)"; exit 1; }
mkdir -p "$OUT"
[ -f "$OUT/frasil-cheatsheet.pdf" ] || : > "$OUT/frasil-cheatsheet.pdf"      # the page links to both: the first
[ -f "$OUT/frasil-cheatsheet.png" ] || : > "$OUT/frasil-cheatsheet.png"      # build needs them there
tools/website.sh >/dev/null

"$PY" website/serve.py build/website/frasil "$PORT" >/dev/null 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null' EXIT INT TERM
URL="http://localhost:$PORT/cheatsheet/"
"$PY" - "$URL" <<'EOF'
import sys, time, urllib.request
for _ in range(100):
    try:
        urllib.request.urlopen(sys.argv[1], timeout=1); break
    except Exception:
        time.sleep(.1)
else:
    raise SystemExit("cheatsheet.sh: the preview server did not answer")
EOF

TMP=$(mktemp -d)
"$CHROME" --headless=new --disable-gpu --user-data-dir="$TMP/pdf" --no-pdf-header-footer --virtual-time-budget=10000 \
    --print-to-pdf="$OUT/frasil-cheatsheet.pdf" "$URL" >/dev/null 2>&1
"$CHROME" --headless=new --disable-gpu --user-data-dir="$TMP/png" --hide-scrollbars --window-size=1400,990 \
    --force-device-scale-factor=1.7714286 --virtual-time-budget=10000 \
    --screenshot="$OUT/frasil-cheatsheet.png" "$URL#sheet" >/dev/null 2>&1
rm -rf "$TMP"

# one page, A4 landscape; the PNG at its size
"$PY" - "$OUT/frasil-cheatsheet.pdf" "$OUT/frasil-cheatsheet.png" <<'EOF'
import re, sys
from PIL import Image
pdf = open(sys.argv[1], "rb").read()
pages = len(re.findall(rb"/Type\s*/Page(?![a-zA-Z])", pdf))
box = re.search(rb"/MediaBox\s*\[\s*0\s+0\s+([\d.]+)\s+([\d.]+)\s*\]", pdf)
w, h = (float(box.group(1)) / 72 * 25.4, float(box.group(2)) / 72 * 25.4) if box else (0, 0)
png = Image.open(sys.argv[2]).size
print(f"cheatsheet  {sys.argv[1]}  {pages} page, {w:.0f} x {h:.0f} mm, {len(pdf) / 1e3:.0f} kB")
print(f"cheatsheet  {sys.argv[2]}  {png[0]} x {png[1]}")
if pages != 1 or round(w) != 297 or round(h) != 210 or png != (2480, 1754):
    raise SystemExit("cheatsheet.sh: not one A4 landscape page, or the PNG not 2480 x 1754: the sheet overflows?")
if b"BarlowSemiCondensed" not in pdf:
    raise SystemExit("cheatsheet.sh: the PDF has no Barlow Semi Condensed: the web fonts did not load (offline?)")
EOF
tools/website.sh >/dev/null
echo "website  build/website/frasil carries them; links ok"
