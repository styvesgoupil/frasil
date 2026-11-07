#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# ENERGY's measure (engine/tools/energy.c): the drones alone read calm (0 .. 100), with a fast deep LFO too (what it doesn't
# count); a busy sequence (OPEN WATER: GLINT's and the bass's sequences at 118 BPM) high; the same piece the same numbers,
# from a file or stdin, twice; a stream's gestures counted; its calibration energy-calibration.json's; not a piece refused.
#   engine/tools/test.sh [OUT_DIR]       (engine/tests/run.sh runs it)
set -e
cd "$(dirname "$0")/../.."
OUT="${1:-build/engine}"
engine/tools/build.sh "$OUT" > /dev/null
DSP="engine/dsp/frasil.c engine/dsp/glint.c engine/dsp/bass.c engine/dsp/nuage.c"
CF="-O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wno-unused-function -Iengine -Iengine/dsp"
${CC:-cc} $CF -o "$OUT/energy_scenes" engine/tests/energy_scenes.c engine/frasil_engine.c $DSP -lm
"$OUT/energy_scenes" "$OUT"
E="$OUT/energy"
TAKE=website/stream/test-programme/test-take-1.frstream
i=0
while "$E" --factory $i > "$OUT/energy-f.json" 2> /dev/null; do
    grep -q '"name":"OPEN WATER"' "$OUT/energy-f.json" && break
    i=$((i + 1))
done
"$E" "$OUT/drones.frscene" > "$OUT/energy-drones.json"
"$E" "$OUT/drones-lfo.frscene" > "$OUT/energy-lfo.json"
"$E" "$OUT/sharp.frscene" > "$OUT/energy-sharp.json"
"$E" "$OUT/soft.frscene" > "$OUT/energy-soft.json"
i=0; : > "$OUT/energy-factory.jsonl"
while "$E" --factory $i >> "$OUT/energy-factory.jsonl" 2> /dev/null; do i=$((i + 1)); done
"$E" "$TAKE" > "$OUT/energy-take.json"
"$E" - < "$TAKE" > "$OUT/energy-take2.json"
"$E" "$TAKE" > "$OUT/energy-take3.json"
"$E" --factory 0 > "$OUT/energy-f0.json"
"$E" --factory 0 > "$OUT/energy-f0b.json"
"$E" "$TAKE" --in 60000 --out 120000 > "$OUT/energy-part.json"
r=0; "$E" engine/tools/energy.c > /dev/null 2>&1 || r=$?
python3 - "$OUT" "$r" <<'PY'
import json, sys
out, refused = sys.argv[1], int(sys.argv[2])
j = lambda n: json.load(open(f"{out}/energy-{n}.json"))
cal = json.load(open("engine/tools/energy-calibration.json"))
d, l, f, t, p = j("drones"), j("lfo"), j("f"), j("take"), j("part")
fails = []
def check(ok, what):
    print(("  ok " if ok else "FAIL ") + what)
    ok or fails.append(what)
check(d["G"] == d["B"] == 0 and d["energy"] <= 100, f"the drones alone: G {d['G']} B {d['B']} D {d['D']}, energy {d['energy']} (0 .. 100)")
check(l["energy"] <= 100, f"the same with LFO 1 fast and deep on FILTER: energy {l['energy']} (not counted: 0 .. 100)")
check(f["name"] == "OPEN WATER" and f["energy"] >= 800, f"a busy sequence, {f['name']} at {f['bpm']} BPM: energy {f['energy']} (800 ..)")
check(open(f"{out}/energy-take.json").read() == open(f"{out}/energy-take2.json").read() == open(f"{out}/energy-take3.json").read()
      and open(f"{out}/energy-f0.json").read() == open(f"{out}/energy-f0b.json").read(), "the same piece, the same numbers (a file, stdin, twice)")
check(t["kind"] == "stream" and t["H"] > 0 and p["in"] == 60000 and p["out"] == 120000 and p["H"] > 0, f"a stream's gestures: H {t['H']} a minute ({p['H']} in 1:00 .. 2:00)")
check(t["r0"] == cal["r0"] and t["r98"] == cal["r98"], f"its calibration energy-calibration.json's (r0 {cal['r0']}, r98 {cal['r98']})")
check(refused == 2, f"not a piece: refused (exit {refused})")
sh, so = j("sharp"), j("soft")
check(sh["G"] > 0 and so["G"] < sh["G"] / 10 and so["B"] <= sh["B"] / 10 and so["energy"] < sh["energy"] / 2,
      f"a strike that swells in (GLINT's and the bass's ATTACK slowest): G {sh['G']} -> {so['G']}, B {sh['B']} -> {so['B']}, energy {sh['energy']} -> {so['energy']}")
fac = {m["name"]: m["energy"] for m in map(json.loads, open(f"{out}/energy-factory.jsonl"))}
nr, rs, wh, lu = fac["NORTH RIVER"], fac["RESILIENCE"], fac["WHITEOUT"], fac["LUMIERE NOCTURNE"]
check(abs(nr - rs) <= 50, f"the owner's listening: NORTH RIVER {nr} and RESILIENCE {rs} close (50 at most apart)")
check(wh > lu, f"the owner's listening: WHITEOUT {wh} above LUMIERE NOCTURNE {lu}")
print("energy: " + ("FAILED" if fails else "ok"))
sys.exit(1 if fails else 0)
PY
