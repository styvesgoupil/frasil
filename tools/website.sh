#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# FRASIL's website, assembled: build/website/frasil/ is the folder to upload to frasilsynth.com (nimbus:
# /var/www/frasilsynth.com; rsync -rlcz --delete --exclude /stats/ ...: stats/ there is the download count, written on
# nimbus by tools/stats/downloads.sh, not part of the build).
#   tools/website.sh             assemble it (website/assemble.py: pages, images, the emulator, a link check)
#   tools/website.sh --serve     and serve it at http://localhost:8047/ (at the root, as frasilsynth.com does)
#   tools/website.sh --river     and serve the River (build/website/river/: river.frasilsynth.com) at
#                                http://localhost:8048/
#   tools/website.sh --no-emu    without the emulator (the play page then says so); otherwise it is required
# The play page carries the browser emulator as ./build.sh --web last made it (app/build/emu): build it first for
# the current version. The River's schema comes from the simulator: ./build.sh --host first. The release status shown
# on the site is in website/site.toml. The River is uploaded to river.frasilsynth.com (nimbus:
# /var/www/river.frasilsynth.com).
set -e
cd "$(dirname "$0")/.."
PY="${PYTHON:-.venv/bin/python}"
[ -x "$PY" ] || PY=python3
"$PY" -c 'import PIL' 2>/dev/null || { echo "website.sh: $PY has no Pillow (python3 -m venv .venv && .venv/bin/pip install Pillow)"; exit 1; }
"$PY" website/assemble.py build/website $(for a in "$@"; do [ "$a" = --no-emu ] && echo --no-emu; done)
if [ "${1:-}" = "--serve" ]; then
    exec "$PY" website/serve.py build/website/frasil "${PORT:-8047}"
fi
if [ "${1:-}" = "--river" ]; then
    exec "$PY" website/serve.py build/website/river "${PORT:-8048}"
fi
