#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# FRASIL's website, assembled: build/website/frasil/ is the folder to upload to styves.dev/frasil/.
#   tools/website.sh             assemble it (website/assemble.py: pages, images, the emulator, a link check)
#   tools/website.sh --serve     and serve build/website/ at http://localhost:8047/frasil/ (the sub-path, for real)
#   tools/website.sh --no-emu    without the emulator (the play page then says so); otherwise it is required
# The play page carries the browser emulator as ./build.sh --web last made it (app/build/emu): build it first for
# the current version. The release status shown on the site is in website/site.toml.
set -e
cd "$(dirname "$0")/.."
PY="${PYTHON:-.venv/bin/python}"
[ -x "$PY" ] || PY=python3
"$PY" -c 'import PIL' 2>/dev/null || { echo "website.sh: $PY has no Pillow (python3 -m venv .venv && .venv/bin/pip install Pillow)"; exit 1; }
"$PY" website/assemble.py build/website
if [ "${1:-}" = "--serve" ]; then
    exec "$PY" website/serve.py build/website "${PORT:-8047}" frasil
fi
