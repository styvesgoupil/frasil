#!/bin/bash
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# Where the engine's time goes (macOS sample, a bench built without inlining so each function shows):
#   tools/perf/profile.sh [CASE]     (default voices8b0: 8 drone voices, every layer, the bass)
cd "$(dirname "$0")/../.."
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
cc -O2 -fno-inline -fno-vectorize -fno-slp-vectorize -ffp-contract=off -std=c99 -w -DPLAT_HOST -Ibase/include \
    -Iapp/dsp -o "$W/bench" tools/bench/bench.c app/dsp/*.c -lm
"$W/bench" cost "${1:-voices8b0}" 600 >/dev/null & P=$!
sleep 1
/usr/bin/sample $P 8 -file "$W/prof.txt" >/dev/null 2>&1
kill $P 2>/dev/null
grep -A28 "Sort by top of stack" "$W/prof.txt" | sed -n 2,28p | grep -v "^ *0x\|Binary\|^$"
