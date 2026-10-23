#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# Instructions per sample of each part of the DSP (macOS: /usr/bin/time -l), against a 0-second run of the same.
# The FM-1's budget, from X0X's measurements (about 115 % load at 3500 host instructions a sample): ~3000 a sample
# is the whole core; aim for 2000 in all.
set -e
cd "$(dirname "$0")/../.."
B=build/bench/bench
SEC=${SEC:-20}
instr() { /usr/bin/time -l "$@" 2>&1 >/dev/null | awk '/instructions retired/ {print $1}'; }
for what in ${*:-reverb shimmer}; do
    base=$(instr $B cost "$what" 0)
    full=$(instr $B cost "$what" "$SEC")
    echo "$what: $(( (full - base) / (SEC * 44100) )) instructions a sample"
done
