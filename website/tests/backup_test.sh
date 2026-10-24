#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# The backup page's model against FRASIL's own backups: the simulator stores a scene and backs up; the page (as
# assembled) puts that scene into two other slots (one with a bass's sequence of its own) and rebuilds the image; the
# simulator restores it and backs up again; the page checks it byte for byte (website/tests/backup_test.mjs). Then a
# backup from before the bass's sequence (1.1's test builds: two objects, no OBJ_BSEQ), one from before BASS's DECAY,
# SUSTAIN and PULSE (FRSh, FRSH), and one from before GLINT's RELEASE (FRSj, FRSI: SCATTER where it is), restored: made
# over by the FM-1.
set -e
cd "$(dirname "$0")/../.."
PY="${PYTHON:-.venv/bin/python}"
[ -x "$PY" ] || PY=python3
T="${TMPDIR:-/tmp}/frasil-backup-test.$$"
mkdir -p "$T"
trap 'rm -rf "$T"' EXIT
"$PY" website/assemble.py "$T/site" --no-emu >/dev/null
export BACKUP_PAGE="$T/site/river/index.html"
HOST=app/build/host/frasil_host
cat > "$T/make.txt" <<'S'
wait 600
# slot 15: two drones latched, stored (SCENES' K1 on it, SAVE held and REC)
tapkey 5
tapkey 9
wait 100
tap SAVE
wait 30
turn K1 -20
wait 100
turn K1 15
wait 30
press SAVE
tap REC
release SAVE
wait 50
expect used14 1
press PLAY
wait 650
release PLAY
wait 50
backup out:device.frasil
S
cat > "$T/check.txt" <<'S'
wait 600
restore out:device.frasil
expect restore_rc 0
restore out:patched.frasil
expect restore_rc 0
expect used2 1
expect used6 1
expect used14 1
backup out:after.frasil
restore out:old10.frasil
expect restore_rc 0
backup out:after10r.frasil
restore out:old11.frasil
expect restore_rc 0
expect storeold 3
backup out:after11r.frasil
expect storeold 0
restore out:old12.frasil
expect restore_rc 0
expect storeold 3
backup out:after12r.frasil
restore out:old13.frasil
expect restore_rc 0
expect storeold 1
backup out:after13r.frasil
S
"$HOST" "$T/make.txt" "$T" | grep -v "^  ok" || true
node website/tests/backup_test.mjs make "$T"
"$HOST" "$T/check.txt" "$T" | grep -v "^  ok" || true
node website/tests/backup_test.mjs check "$T"
