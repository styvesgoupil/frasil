#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# The screen mirror's checks (card #279: fm1.toml mirror = true; GitHub issue #9). mirror.txt: every screen FRASIL
# draws, as a viewer rebuilds it (app/tests/mirror_check.py), against the screen, pixel for pixel. mirror_slow.txt: at
# USB's own pace, the river moving, what a viewer reads (the header, the footer, an edit page's values) within half a
# second of a turn, alongside a PERFORM page and a screen reader. mirror_remote.py: the viewer's buttons and knobs.
# mirror_play.txt twice, a viewer on and none: the renders and the MIDI sent the same, bit for bit.
#   app/tests/mirror.sh [OUTDIR]          (after ./build.sh --host)
set -e
cd "$(dirname "$0")/../.."
H=app/build/host/frasil_host
PY="${PYTHON:-.venv/bin/python}"
[ -x "$PY" ] || PY=python3
OUT="${1:-$(mktemp -d)}"
mkdir -p "$OUT/screens" "$OUT/slow" "$OUT/on" "$OUT/off"

# every screen: each fbdump of mirror.txt against its stream, the CORNER! read alone (it moves all over)
$H app/tests/mirror.txt "$OUT/screens" > "$OUT/screens.log" || { cat "$OUT/screens.log"; echo "mirror: screens failed"; exit 1; }
grep -E '^ *(ok|FAIL)( |$)' "$OUT/screens.log" || true
specs="$(sed -n 's/^fbdump \(.*\)\.raw$/\1/p' app/tests/mirror.txt | sed 's/^party$/party:decodes/')"
# shellcheck disable=SC2086
"$PY" app/tests/mirror_check.py --png "$OUT/screens" $specs || { echo "mirror: a screen differs"; exit 1; }

# USB's pace: the rows a viewer reads (the river moving above an edit page's values)
$H app/tests/mirror_slow.txt "$OUT/slow" > "$OUT/slow.log" || { cat "$OUT/slow.log"; echo "mirror: slow failed"; exit 1; }
grep -E '^ *(ok|FAIL)( |$)' "$OUT/slow.log" || true
"$PY" app/tests/mirror_check.py --png "$OUT/slow" logo:covers river:0-31,176-239 edit:0-31,96-239 edit_back:0-31,96-239 popup \
    popup_gone:0-31,96-239 help together:0-31,96-239 still still_edit || { echo "mirror: slow differs"; exit 1; }

# the viewer's buttons and knobs working FRASIL's (the simulator as an FM-1 over USB)
"$PY" app/tests/mirror_remote.py "$OUT/remote" || { echo "mirror: the remote failed"; exit 1; }

# nothing that plays changes: the same script with no viewer (each "mirror" line a millisecond, as it takes one)
$H app/tests/mirror_play.txt "$OUT/on" > "$OUT/on.log" || { cat "$OUT/on.log"; echo "mirror: ON failed"; exit 1; }
sed -e 's/^mirror out:.*$/wait 1/' -e 's/^mirror off$/wait 1/' -e '/^mirror_rate /d' app/tests/mirror_play.txt > "$OUT/off.txt"
$H "$OUT/off.txt" "$OUT/off" > "$OUT/off.log" || { cat "$OUT/off.log"; echo "mirror: OFF failed"; exit 1; }
on="$(grep renderhash "$OUT/on.log")"
off="$(grep renderhash "$OUT/off.log")"
echo "mirror: ON $on"
echo "mirror: OFF $off"
[ -n "$on" ] && [ "$on" = "$off" ] || { echo "FAIL mirror: the renders differ, a viewer on and none"; exit 1; }
cmp -s "$OUT/on/midi_out.txt" "$OUT/off/midi_out.txt" || { echo "FAIL mirror: the MIDI sent differs"; exit 1; }
[ -s "$OUT/on/midi_out.txt" ] || { echo "FAIL mirror: no MIDI sent (MIDI's keys out)"; exit 1; }
grep '^mirror:' "$OUT/on.log"
echo "  ok mirror play: the render and the MIDI sent the same, a viewer on and none"
