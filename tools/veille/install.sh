#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# The veille's pass, to the M3 (from the Mac): its files, its two secrets (never shown), its launchd job.   tools/veille/install.sh [--secrets]
set -eu
cd "$(dirname "$0")"
H="${VEILLE_HOST:-m3}"
ssh "$H" 'mkdir -p ~/.local/share/frasil-veille ~/.local/state/frasil-veille ~/.config/frasil && chmod 700 ~/.config/frasil'
rsync -a collect.py ingest.py prompt.md run.sh "$H:.local/share/frasil-veille/"
if [ "${1:-}" = "--secrets" ]; then
    scp -q ~/.config/frasil/veille.token ~/.config/frasil/youtube.key "$H:.config/frasil/"
    ssh "$H" 'chmod 600 ~/.config/frasil/veille.token ~/.config/frasil/youtube.key'
fi
scp -q com.styves.frasil-veille.plist "$H:Library/LaunchAgents/"
ssh "$H" 'launchctl bootout gui/$(id -u)/com.styves.frasil-veille 2>/dev/null; launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/com.styves.frasil-veille.plist; launchctl print gui/$(id -u)/com.styves.frasil-veille | grep -E "state =|path =" | head -3'
