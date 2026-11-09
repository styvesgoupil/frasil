#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# The engine alone:
# - engine/dsp/fastmath.h, fm1-base's (its guard aside);
# - its API (engine/tests/api: frasil_engine.h as an iPad or a plug-in calls it; in C99 and C11) and its inside (engine/tests/inside:
#   the clock past 2^32 ms, a note under the scale, the engine opened again, the tracks stopped and played again in
#   phase, a chord's kind), also under AddressSanitizer and UBSan;
# - the calls any thread may make, from a UI's thread while the audio thread plays, under ThreadSanitizer
#   (engine/tests/threads);
# - against the FM-1's simulator (./build.sh --host first): engine/tests/alone runs app/tests/engine_alone.txt, the
#   simulator too, every renderhash the same, bit for bit;
# - at 48 kHz: the same script second by second beside 44.1 kHz's (engine/tests/rates.py), and measured
#   (engine/tests/rate, at 48 and 96 kHz: a pitch, GLINT's echo, CLOUD's RT60, LOFI TAPE's delay and flutter,
#   MASTER FILTER's 3 dB point, MASTER EQ's MID FREQ).
# - a stream joined at any place (engine/tests/skip: frasil_stream's player, a skip's messages against a render's, every
#   factory scene, with the controls that must differ);
# - a stream from before ARP SNAP as its engine played it (engine/tests/old_streams: engine/tests/old-streams/'s takes, made
#   on the engine just before it, bit for bit from the top and joined; a newer one with its own ARP SNAP); 3.0.2-b5's,
#   "FRS{" (engine/tests/old-streams-3.0.2/), the same;
# - ANCHOR (engine/tests/anchor: set, BACK and MORPH bit for bit, a hand, a STORE and a scene during it; a take with it
#   journaled and played on air, every sample the live one's, joined inside its MORPH), also under ASan and UBSan;
# - a performance heard live (engine/tests/live: fst_op, each op by itself, a scene by its bytes: every sample, setting
#   and note a replay of its file's; ops not known or too short ignored or refused, counted), also under ASan and UBSan;
# - a listener opened on a snap (engine/tests/snap: the running state, card #131: opened 30 s in, the same ops after
#   in their milliseconds, the brain bit for bit the first engine's to the end, the sound within the buffers'; without
#   the state, elsewhere; late ops then the snap at its place, the brain again; on a MIDI clock, card #123's, the same),
#   also under ASan and UBSan;
# - ENERGY, FRASIL STREAM's measure of a piece (engine/tools/test.sh: the drones alone calm, a busy sequence high, the
#   same piece the same numbers);
#   engine/tests/run.sh [OUT_DIR]
set -e
cd "$(dirname "$0")/../.."
OUT="${1:-build/engine}"
mkdir -p "$OUT"
DSP="engine/dsp/frasil.c engine/dsp/glint.c engine/dsp/bass.c engine/dsp/nuage.c"
CF="-O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wno-unused-function -Iengine -Iengine/dsp"
SAN="-g -O1 -fsanitize=address,undefined -fno-sanitize=shift-base -fno-sanitize-recover=all"

if [ -f base/include/fastmath.h ]; then
    python3 - <<'EOF'
import sys
mine = open("engine/dsp/fastmath.h").read().split("\n")
theirs = open("base/include/fastmath.h").read().split("\n")
i = mine.index("#ifndef FM_PI")
mine = mine[:i - 2] + mine[i + 1:]                 # (the copy's note and its guard)
while mine and mine[-1] in ("", "#endif /* FM_PI */"):
    mine.pop()
while theirs and theirs[-1] == "":
    theirs.pop()
if mine != theirs:
    sys.exit("fastmath.h: engine/dsp's is not fm1-base's")
print("fastmath.h: fm1-base's, its guard aside")
EOF
fi

V="$(tr -d ' \n' < VERSION)"
grep -q "^#define FRS_VERSION \"$V\"" engine/frasil_engine.h || { echo "run.sh: frasil_engine.h's FRS_VERSION is not VERSION's ($V)"; exit 1; }
echo "FRS_VERSION: VERSION's ($V)"

${CC:-cc} $CF -o "$OUT/api" engine/tests/api.c engine/frasil_engine.c $DSP -lm
${CC:-cc} $CF -std=c11 -o "$OUT/api-c11" engine/tests/api.c engine/frasil_engine.c $DSP -lm   # (C11's atomics)
${CC:-cc} $CF -o "$OUT/inside" engine/tests/inside.c $DSP -lm
${CC:-cc} $CF -o "$OUT/alone" engine/tests/alone.c $DSP -lm
${CC:-cc} $CF -o "$OUT/rate" engine/tests/rate.c $DSP -lm
${CC:-cc} $CF $SAN -o "$OUT/api-san" engine/tests/api.c engine/frasil_engine.c $DSP -lm
${CC:-cc} $CF $SAN -o "$OUT/inside-san" engine/tests/inside.c $DSP -lm
${CC:-cc} $CF -fsanitize=thread -g -o "$OUT/threads" engine/tests/threads.c engine/frasil_engine.c $DSP -lm -lpthread
${CC:-cc} $CF -DFRASIL_EV_LOG=skip_log -o "$OUT/skip" engine/tests/skip.c engine/frasil_stream.c engine/frasil_engine.c $DSP -lm
${CC:-cc} $CF -o "$OUT/old_streams" engine/tests/old_streams.c engine/frasil_stream.c engine/frasil_engine.c $DSP -lm
${CC:-cc} $CF -o "$OUT/anchor" engine/tests/anchor.c engine/frasil_stream.c engine/frasil_engine.c $DSP -lm
${CC:-cc} $CF $SAN -o "$OUT/anchor-san" engine/tests/anchor.c engine/frasil_stream.c engine/frasil_engine.c $DSP -lm
${CC:-cc} $CF -o "$OUT/live" engine/tests/live.c engine/frasil_stream.c engine/frasil_engine.c $DSP -lm
${CC:-cc} $CF $SAN -o "$OUT/live-san" engine/tests/live.c engine/frasil_stream.c engine/frasil_engine.c $DSP -lm
${CC:-cc} $CF -o "$OUT/snap" engine/tests/snap.c engine/frasil_stream.c $DSP -lm
${CC:-cc} $CF $SAN -o "$OUT/snap-san" engine/tests/snap.c engine/frasil_stream.c $DSP -lm
for t in api api-c11 inside api-san inside-san rate threads skip old_streams anchor anchor-san live live-san snap snap-san; do
    "$OUT/$t" > "$OUT/$t.txt" 2>&1 || { cat "$OUT/$t.txt"; echo "run.sh: $t failed"; exit 1; }
    tail -1 "$OUT/$t.txt"
done
grep -h 'Hz' "$OUT/rate.txt"
engine/tools/test.sh "$OUT" > "$OUT/energy.txt" 2>&1 || { cat "$OUT/energy.txt"; echo "run.sh: energy failed"; exit 1; }
tail -1 "$OUT/energy.txt"

[ -x app/build/host/frasil_host ] || { echo "run.sh: no simulator (./build.sh --host)"; exit 1; }
app/build/host/frasil_host app/tests/engine_alone.txt "$OUT" | grep renderhash > "$OUT/host.hash"
"$OUT/alone" app/tests/engine_alone.txt 44100 "$OUT/alone-44100.wav" > "$OUT/alone.hash"
if cmp -s "$OUT/host.hash" "$OUT/alone.hash"; then
    echo "alone: the simulator's renders, bit for bit ($(wc -l < "$OUT/host.hash" | tr -d ' ') renderhash)"
else
    echo "alone: NOT the simulator's renders"
    diff "$OUT/host.hash" "$OUT/alone.hash" || true
    exit 1
fi
"$OUT/alone" app/tests/engine_alone.txt 48000 "$OUT/alone-48000.wav" > /dev/null
python3 engine/tests/rates.py "$OUT/alone-44100.wav" "$OUT/alone-48000.wav"
# an FM-1's STATE (the simulator's, GLINT's arpeggio at random, LFOs, a chord, a key down) into the engine alone (card #131)
printf '%s\n' "keysstay 1" "wait 600" "setp 166 1" "setp 123 1" "setp 45 3" "setp 48 60" "setp 94 5" "setp 95 60" \
    "setp 96 90" "tapkey 0" "tapkey 4" "tapkey 7" "setp 166 0" "wait 800" "key 9 down 90 80" "wait 50" "pfhello 2" \
    "pfnone" "pfstate" "wait 80" "pfsave $OUT/fm1-state.bin" "key 9 up" > "$OUT/fm1_state.txt"
app/build/host/frasil_host "$OUT/fm1_state.txt" "$OUT" > "$OUT/fm1_state.log" 2>&1 && ! grep -q '^FAIL' "$OUT/fm1_state.log" ||
    { cat "$OUT/fm1_state.log"; echo "run.sh: the simulator's STATE not saved"; exit 1; }
for t in snap snap-san; do
    "$OUT/$t" "$OUT/fm1-state.bin" > "$OUT/$t-fm1.txt" 2>&1 || { cat "$OUT/$t-fm1.txt"; echo "run.sh: $t (an FM-1's STATE) failed"; exit 1; }
done
grep -h "an FM-1's STATE" "$OUT/snap-fm1.txt"
tail -1 "$OUT/snap-fm1.txt"
