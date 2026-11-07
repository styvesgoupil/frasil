#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# The screensaver's checks (saver.txt), then saver_play.txt twice, SCREENSAVER ON and OFF: the renders and the MIDI sent
# the same, bit for bit (it changes nothing that plays).
#   app/tests/saver.sh [OUTDIR]          (after ./build.sh --host)
set -e
cd "$(dirname "$0")/../.."
H=app/build/host/frasil_host
OUT="${1:-$(mktemp -d)}"
mkdir -p "$OUT/on" "$OUT/off"
$H app/tests/saver.txt "$OUT" > "$OUT/saver.log" || { grep FAIL "$OUT/saver.log"; echo "saver: checks failed"; exit 1; }
$H app/tests/saver_play.txt "$OUT/on" > "$OUT/on.log" || { grep FAIL "$OUT/on.log"; echo "saver: ON failed"; exit 1; }
sed -e 's/^setp 226 1$/setp 226 0/' -e 's/^expect saver 1$/expect saver 0/' -e 's/^expect saver_corners 1$/expect saver_corners 0/' \
    app/tests/saver_play.txt > "$OUT/off.txt"
$H "$OUT/off.txt" "$OUT/off" > "$OUT/off.log" || { grep FAIL "$OUT/off.log"; echo "saver: OFF failed"; exit 1; }
on="$(grep renderhash "$OUT/on.log")"
off="$(grep renderhash "$OUT/off.log")"
echo "saver: ON $on"
echo "saver: OFF $off"
[ -n "$on" ] && [ "$on" = "$off" ] || { echo "saver: the renders differ"; exit 1; }
cmp -s "$OUT/on/midi_out.txt" "$OUT/off/midi_out.txt" || { echo "saver: the MIDI sent differs"; exit 1; }
echo "saver: ok ($(grep -c '^  ok' "$OUT/saver.log") checks; the render and the MIDI sent the same, ON and OFF)"
