#!/bin/bash
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# The DSP bench as it was at a commit (its engine/dsp, before the engine its app/dsp, and its tools/bench/bench.c; at
# the FM-1's one rate, FRASIL_FS_CONST), for comparing an engine with an earlier one: tools/perf/bench_at.sh REF OUT   then   OUT cost voices8b0 10 / OUT fx base old.wav
#   tools/perf/cost.sh BENCH CASE...  (below: the instructions a sample, as tools/bench/cost.sh counts them)
# NOW=1: today's bench.c on REF's engine (the cases added since). A null test: DRY=1 OUT fx WHICH (no cloud: its
# int16 lines turn any change upstream into a reshuffle of their own rounding, some -75 dB, hiding the change's own)
set -e
cd "$(dirname "$0")/../.."
REF=$1 OUT=$2
[ -n "$REF" ] && [ -n "$OUT" ] || { echo "usage: bench_at.sh REF OUT"; exit 2; }
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
D=engine/dsp
git cat-file -e "$REF:engine/dsp/frasil.c" 2>/dev/null || D=app/dsp
for f in $(git ls-tree --name-only "$REF" "$D/"); do git show "$REF:$f" > "$W/$(basename "$f")"; done
if [ -n "$NOW" ]; then cp tools/bench/bench.c "$W/bench_ref.c"; else git show "$REF:tools/bench/bench.c" > "$W/bench_ref.c"; fi
${CC:-cc} -O2 -fno-vectorize -fno-slp-vectorize -ffp-contract=off -std=c99 -w -DPLAT_HOST -DFRASIL_FS_CONST -Ibase/include -I"$W" \
    -o "$OUT" "$W/bench_ref.c" $(ls "$W"/*.c | grep -v bench_ref) -lm
echo "bench at $REF: $OUT"
