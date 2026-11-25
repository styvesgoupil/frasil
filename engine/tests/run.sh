#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# The engine alone:
# - engine/dsp/fastmath.h, fm1-base's (its guard aside);
# - its API (engine/tests/api: frasil_engine.h as an iPad or a plug-in calls it; in C99 and C11) and its inside (engine/tests/inside:
#   the clock past 2^32 ms, a note under the scale, the engine opened again, the tracks stopped and played again in
#   phase, a chord's kind, the sound's CCs: cards #256's and #265's CC_MAP), also under AddressSanitizer and UBSan;
# - the calls any thread may make, from a UI's thread while the audio thread plays, under ThreadSanitizer
#   (engine/tests/threads);
# - against the FM-1's simulator (./build.sh --host first): engine/tests/alone runs app/tests/engine_alone.txt, the
#   simulator too, every renderhash the same, bit for bit (alone built as the app builds the engine, with the LEAD:
#   FRS_LEAD and dsp/lead.c, card #144; the rest here without it, as the iPad builds it, and live, snap, anchor with it
#   too, as STREAM's stream.wasm since card #152);
# - at 48 kHz: the same script second by second beside 44.1 kHz's (engine/tests/rates.py), and measured
#   (engine/tests/rate, at 48 and 96 kHz: a pitch, GLINT's echo, CLOUD's RT60, LOFI TAPE's delay and flutter,
#   MASTER FILTER's 3 dB point, MASTER EQ's MID FREQ).
# - a stream joined at any place (engine/tests/skip: frasil_stream's player, a skip's messages against a render's, every
#   factory scene, with the controls that must differ);
# - a stream from before ARP SNAP as its engine played it (engine/tests/old_streams: engine/tests/old-streams/'s takes, made
#   on the engine just before it, bit for bit from the top and joined; a newer one with its own ARP SNAP); 3.0.2-b5's,
#   "FRS{" (engine/tests/old-streams-3.0.2/), the same; 3.2's, "FRS}" (engine/tests/old-streams-3.2/), the same, their LEAD
#   MOSS; 3.3.1's timed MORPHs (engine/tests/old-streams-3.3/), the same, card #202's MORPH moving every one;
# - a scene's MORPH (engine/tests/morph, card #202: NORTH RIVER to LONG NIGHT and back over 2:00, the notes let go and
#   played over it, not on the scene left's envelope; MASTERING, CLOUD and MASTER EQ gliding; its mark in the state, a
#   stream without it 3.3.1's MORPH), also under ASan and UBSan;
# - ANCHOR (engine/tests/anchor: set, BACK and MORPH bit for bit, a hand, a STORE and a scene during it; a take with it
#   journaled and played on air, every sample the live one's, joined inside its MORPH), also under ASan and UBSan;
# - a performance heard live (engine/tests/live: fst_op, each op by itself, a scene by its bytes: every sample, setting
#   and note a replay of its file's; ops not known or too short ignored or refused, counted), also under ASan and UBSan;
# - a listener opened on a snap (engine/tests/snap: the running state, card #131: opened 30 s in, the same ops after
#   in their milliseconds, the brain bit for bit the first engine's to the end, the sound within the buffers'; without
#   the state, elsewhere; late ops then the snap at its place, the brain again; on a MIDI clock, card #123's, the same),
#   also under ASan and UBSan;
# - the LEAD (engine/tests/lead, card #152, with it: frs_lead_*, its ops, a take with it in "FRS~" on air every sample
#   the same take live, an "FRS}" stream with MOSS, the running state's LEAD part: a listener opened on it the artist's
#   brain and voices, one on the first part alone none stuck), also under ASan and UBSan;
# - MIDI OUT (engine/tests/mout, card #192, built as the iPad builds it: FRS_MIDI_OUT, engine/mout.c): its keys, its
#   arpeggio, its sequence and CLOCK OUT on the grid, what goes out and when, nothing sounding; card #219's: its KEYS'
#   chords, CLOCK OUT's Start and Stop after a skip, NOTES OUT (card #201: the sound bit for bit with it), the LEAD's
#   chord from frs_key_notes; also under ASan and UBSan (the API's card #219 calls in engine/tests/api: ATTACK SHAPE,
#   MORPH's progress, frs_key_notes, SONG POSITION for a host);
# - ATTACK SHAPE (engine/tests/shape, card #122, with the LEAD): CURVE bit for bit the engine's of before, LINE straight up
#   on each envelope, the state's pair; takes made with LINE (engine/tests/line-streams/) heard with it here, and read
#   and heard CURVE on history/dev ef2cbb5's engine (engine/tests/shape_old, built on it from git: not in a tree without
#   that commit, said); also under ASan and UBSan;
# - LOFI TAPE's MEDIA (engine/tests/media, card #99): TAPE bit for bit as before them, VINYL and AM bounded and as loud,
#   silent in silent out at NOISE 0, their noise, their dips, a medium changed while it plays; VINYL's record player
#   (card #224: OFF, TAPE, AM and their CUT bit for bit, the needle's drop, its marks each turn, CUT's stop, PLAY in
#   it); also under ASan and UBSan;
#   PHONE (card #218): AM bit for bit as before it (VINYL's own since card #224), PHONE one side in its band, its hiss
#   and hum, its cuts; takes made with it heard on the engines before it (engine/tests/media_old, built from git on
#   history/dev 49bcaa8, PHONE as AM, and on 70c3ad0, before the media, as TAPE: not in a tree without those commits,
#   said);
# - LEAD CH (engine/tests/ldch, card #228, with the LEAD): OFF at the start; the LEAD's MIDI channel, its notes MONO's,
#   POLY's, CHORD's, their pressure and wheels, none hanging (CC 123, CC 120, the channel and MODE turned); the state's
#   pair only while not OFF; a take with it on air every sample the take live; a listener opened on MIDI's chord held,
#   the same brain; what it makes read on 3.4's engine (engine/tests/ldch_old, built from git on c5398e68: not in a
#   tree without that commit, said): the state as at the start byte for byte, its pair not taken, the sound with
#   nothing on the LEAD bit for bit, its take played; 3.4's take with notes on channel 4 (ldch_old make) heard here
#   every sample as 3.4 played it; also under ASan and UBSan;
# - CHORD PADS (engine/tests/chords, card #208, built as FrasilVibe builds it: FRS_CHORDS, engine/chords.c, the LEAD):
#   the chords' words, a move (the drones on its chord, GLINT, the bass, the LEAD moved), ON THE BEAT, GLIDE, RESET,
#   FOLLOW, the running state; and a performance with no move the samples of the same engine without them, bit for bit;
#   also under ASan and UBSan;
# - TRACKS LINK (engine/tests/link, card #212, built as the app builds it: FRS_LEAD, FRS_MIDI_OUT; every note the voices
#   are sent logged, -DFRASIL_EV_LOG=link_log): the offset's degrees; the bass, GLINT's arpeggio and keys, MIDI OUT's
#   keys and arpeggio moved on the drone's note, the drones' chord on the bass's, at the pitches wanted; every note struck
#   let go once, where it went, through the link turned, FOLLOW, ROOT, SCALE, RELEASE ALL, scenes, SPARKLE beside GLINT,
#   GLINT's notes onto one pitch, 40 drones moved at once (none dropped from the voices' queue); the state's pairs, the
#   running state's; also under ASan and UBSan;
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
${CC:-cc} $CF -DFRS_LEAD=1 -o "$OUT/alone" engine/tests/alone.c $DSP engine/dsp/lead.c -lm   # (the app's engine, its
                                          # LEAD's FRS_LEAD as fm1.toml's o2_cflags: the simulator's; card #144)
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
${CC:-cc} $CF -o "$OUT/morph" engine/tests/morph.c engine/frasil_stream.c $DSP -lm
${CC:-cc} $CF $SAN -o "$OUT/morph-san" engine/tests/morph.c engine/frasil_stream.c $DSP -lm
LD="-DFRS_LEAD=1 engine/dsp/lead.c"      # (with the LEAD, as stream.wasm: tools/stream_wasm.sh, card #152)
${CC:-cc} $CF -o "$OUT/lead" engine/tests/lead.c engine/frasil_stream.c $DSP $LD -lm
${CC:-cc} $CF $SAN -o "$OUT/lead-san" engine/tests/lead.c engine/frasil_stream.c $DSP $LD -lm
${CC:-cc} $CF -o "$OUT/live-lead" engine/tests/live.c engine/frasil_stream.c engine/frasil_engine.c $DSP $LD -lm
${CC:-cc} $CF -o "$OUT/snap-lead" engine/tests/snap.c engine/frasil_stream.c $DSP $LD -lm
${CC:-cc} $CF -o "$OUT/anchor-lead" engine/tests/anchor.c engine/frasil_stream.c engine/frasil_engine.c $DSP $LD -lm
${CC:-cc} $CF -o "$OUT/ldch" engine/tests/ldch.c engine/frasil_stream.c $DSP $LD -lm   # (LEAD CH, card #228)
${CC:-cc} $CF $SAN -o "$OUT/ldch-san" engine/tests/ldch.c engine/frasil_stream.c $DSP $LD -lm
LDCH_OLD=""                              # (card #228's: what it makes on 3.4's engine, c5398e68's, as git has it)
if git cat-file -e "c5398e68^{commit}" 2>/dev/null; then
    O="$OUT/c5398e68"
    rm -rf "$O" && mkdir -p "$O" && git archive c5398e68 engine | tar -x -C "$O"
    ${CC:-cc} -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wno-unused-function -I"$O/engine" -I"$O/engine/dsp" \
        -DFRS_LEAD=1 -o "$OUT/ldch-old" engine/tests/ldch_old.c "$O/engine/frasil_stream.c" "$O/engine/frasil_engine.c" \
        "$O/engine/dsp/frasil.c" "$O/engine/dsp/glint.c" "$O/engine/dsp/bass.c" "$O/engine/dsp/nuage.c" \
        "$O/engine/dsp/lead.c" -lm
    LDCH_OLD="ldch-old"
else
    echo "ldch-old: no c5398e68 in this tree (LEAD CH's on 3.4's engine): not run"
fi
${CC:-cc} $CF -o "$OUT/shape" engine/tests/shape.c engine/frasil_stream.c $DSP $LD -lm
${CC:-cc} $CF $SAN -o "$OUT/shape-san" engine/tests/shape.c engine/frasil_stream.c $DSP $LD -lm
SHAPE_OLD=""                             # (card #122's takes on the engine of before it: ef2cbb5's, as git has it)
if git cat-file -e "ef2cbb51^{commit}" 2>/dev/null; then
    O="$OUT/ef2cbb5"
    rm -rf "$O" && mkdir -p "$O" && git archive ef2cbb51 engine | tar -x -C "$O"
    ${CC:-cc} -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wno-unused-function -I"$O/engine" -I"$O/engine/dsp" \
        -DFRS_LEAD=1 -o "$OUT/shape-old" engine/tests/shape_old.c "$O/engine/frasil_stream.c" "$O/engine/frasil_engine.c" \
        "$O/engine/dsp/frasil.c" "$O/engine/dsp/glint.c" "$O/engine/dsp/bass.c" "$O/engine/dsp/nuage.c" \
        "$O/engine/dsp/lead.c" -lm
    SHAPE_OLD="shape-old"
else
    echo "shape-old: no ef2cbb51 in this tree (card #122's takes on the engine of before it): not run"
fi
MO="-DFRS_MIDI_OUT=1"                    # (MIDI OUT, as the iPad builds it: engine/mout.c, card #192)
${CC:-cc} $CF -o "$OUT/mout" engine/tests/mout.c $DSP $LD $MO -lm
${CC:-cc} $CF $SAN -o "$OUT/mout-san" engine/tests/mout.c $DSP $LD $MO -lm
${CC:-cc} $CF -o "$OUT/api-mout" engine/tests/api.c engine/frasil_engine.c $DSP $MO -lm   # (the API's own, MIDI OUT in)
CH="-DFRS_CHORDS=1"                      # (CHORD PADS, as FrasilVibe builds it: engine/chords.c, card #208)
${CC:-cc} $CF -o "$OUT/chords" engine/tests/chords.c $DSP $LD $CH -lm
${CC:-cc} $CF $SAN -o "$OUT/chords-san" engine/tests/chords.c $DSP $LD $CH -lm
${CC:-cc} $CF -o "$OUT/chords-off" engine/tests/chords.c $DSP $LD -lm   # (its performance without them)
${CC:-cc} $CF -o "$OUT/api-chords" engine/tests/api.c engine/frasil_engine.c $DSP $CH -lm
${CC:-cc} $CF -o "$OUT/media" engine/tests/media.c engine/frasil_engine.c engine/frasil_stream.c $DSP -lm   # (LOFI
                                         # TAPE's MEDIA, card #99; its takes for the engines before PHONE, card #218)
${CC:-cc} $CF $SAN -o "$OUT/media-san" engine/tests/media.c engine/frasil_engine.c engine/frasil_stream.c $DSP -lm
MEDIA_OLD=""                             # (PHONE's takes on the engines before it: 49bcaa8's with VINYL and AM, 70c3ad0's
for c in 49bcaa81 70c3ad07; do           # before them, as git has them)
    if git cat-file -e "$c^{commit}" 2>/dev/null; then
        O="$OUT/$c"
        rm -rf "$O" && mkdir -p "$O" && git archive "$c" engine | tar -x -C "$O"
        ${CC:-cc} -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wno-unused-function -I"$O/engine" -I"$O/engine/dsp" \
            -DFRS_LEAD=1 -o "$OUT/media-old-$c" engine/tests/media_old.c "$O/engine/frasil_stream.c" \
            "$O/engine/frasil_engine.c" "$O/engine/dsp/frasil.c" "$O/engine/dsp/glint.c" "$O/engine/dsp/bass.c" \
            "$O/engine/dsp/nuage.c" "$O/engine/dsp/lead.c" -lm
        MEDIA_OLD="$MEDIA_OLD media-old-$c"
    else
        echo "media-old: no $c in this tree (PHONE's takes on the engine of before it): not run"
    fi
done
LK="-DFRASIL_EV_LOG=link_log"            # (TRACKS LINK, card #212: what the voices are sent)
${CC:-cc} $CF $LK -o "$OUT/link" engine/tests/link.c engine/frasil_stream.c $DSP $LD $MO -lm
${CC:-cc} $CF $SAN $LK -o "$OUT/link-san" engine/tests/link.c engine/frasil_stream.c $DSP $LD $MO -lm
for t in api api-c11 inside api-san inside-san rate threads skip old_streams anchor anchor-san live live-san snap snap-san \
    morph morph-san lead lead-san live-lead snap-lead anchor-lead mout mout-san api-mout media media-san ldch-san \
    link link-san chords chords-san chords-off api-chords; do
    "$OUT/$t" > "$OUT/$t.txt" 2>&1 || { cat "$OUT/$t.txt"; echo "run.sh: $t failed"; exit 1; }
    tail -1 "$OUT/$t.txt"
done
if [ "$(head -1 "$OUT/chords.txt")" = "$(head -1 "$OUT/chords-off.txt")" ]; then
    echo "chords: no move, the samples of an engine without them, bit for bit"
else
    echo "chords: NOT the samples of an engine without them"; head -1 "$OUT/chords.txt" "$OUT/chords-off.txt"; exit 1
fi
for t in shape shape-san $SHAPE_OLD; do   # (card #122's: the takes' directory its argument there)
    if [ "$t" = shape-old ]; then "$OUT/$t" engine/tests/line-streams; else "$OUT/$t"; fi > "$OUT/$t.txt" 2>&1 ||
        { cat "$OUT/$t.txt"; echo "run.sh: $t failed"; exit 1; }
    tail -1 "$OUT/$t.txt"
done
if [ -n "$MEDIA_OLD" ]; then             # (card #218's: the takes made here, heard there)
    rm -rf "$OUT/media-takes" && mkdir -p "$OUT/media-takes"
    "$OUT/media" make "$OUT/media-takes" > "$OUT/media-make.txt" 2>&1 ||
        { cat "$OUT/media-make.txt"; echo "run.sh: media make failed"; exit 1; }
    for t in $MEDIA_OLD; do
        if [ "$t" = media-old-49bcaa81 ]; then w=with; else w=before; fi
        "$OUT/$t" $w "$OUT/media-takes" > "$OUT/$t.txt" 2>&1 || { cat "$OUT/$t.txt"; echo "run.sh: $t failed"; exit 1; }
        tail -1 "$OUT/$t.txt"
    done
fi
rm -rf "$OUT/ldch-made" && mkdir -p "$OUT/ldch-made"   # (card #228's: 3.4's take made on 3.4's engine, heard here; made
for t in $LDCH_OLD; do                                 # here, read on 3.4's engine)
    "$OUT/$t" make "$OUT/ldch-made" > "$OUT/$t-make.txt" 2>&1 || { cat "$OUT/$t-make.txt"; echo "run.sh: $t make failed"; exit 1; }
    tail -1 "$OUT/$t-make.txt"
done
"$OUT/ldch" make "$OUT/ldch-made" > "$OUT/ldch.txt" 2>&1 || { cat "$OUT/ldch.txt"; echo "run.sh: ldch failed"; exit 1; }
tail -1 "$OUT/ldch.txt"
for t in $LDCH_OLD; do
    "$OUT/$t" "$OUT/ldch-made" > "$OUT/$t.txt" 2>&1 || { cat "$OUT/$t.txt"; echo "run.sh: $t failed"; exit 1; }
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
# an FM-1's STATE (the simulator's, GLINT's arpeggio at random, LFOs, a chord, a key down, a key on the LEAD: its part
# after the rest, card #152) into the engine alone (card #131): without the LEAD (the first part, the LEAD's left) and
# with it (both)
printf '%s\n' "keysstay 1" "wait 600" "setp 166 1" "setp 123 1" "setp 45 3" "setp 48 60" "setp 94 5" "setp 95 60" \
    "setp 96 90" "tapkey 0" "tapkey 4" "tapkey 7" "setp 166 3" "key 11 down 72 100" "wait 30" "setp 166 0" "wait 800" \
    "key 9 down 90 80" "wait 50" "pfhello 2" "pfnone" "pfstate" "wait 80" "pfsave $OUT/fm1-state.bin" "key 9 up" \
    "key 11 up" > "$OUT/fm1_state.txt"
app/build/host/frasil_host "$OUT/fm1_state.txt" "$OUT" > "$OUT/fm1_state.log" 2>&1 && ! grep -q '^FAIL' "$OUT/fm1_state.log" ||
    { cat "$OUT/fm1_state.log"; echo "run.sh: the simulator's STATE not saved"; exit 1; }
for t in snap snap-san snap-lead; do
    "$OUT/$t" "$OUT/fm1-state.bin" > "$OUT/$t-fm1.txt" 2>&1 || { cat "$OUT/$t-fm1.txt"; echo "run.sh: $t (an FM-1's STATE) failed"; exit 1; }
done
grep -h "an FM-1's STATE" "$OUT/snap-fm1.txt"
tail -1 "$OUT/snap-fm1.txt"
tail -1 "$OUT/snap-lead-fm1.txt"
