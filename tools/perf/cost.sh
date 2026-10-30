#!/bin/bash
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# Instructions a sample of a bench's cases (macOS /usr/bin/time -l, a 10 s run less a 0 s one):
#   tools/perf/cost.sh [BENCH] CASE...    e.g. tools/perf/cost.sh voices8b0 voices8b0f voices8:c
# (BENCH: a path to a bench, else build/bench/bench). The FM-1 does one of these in some 1.7 host-instructions' time:
# measure there (tools/perf/fm1_load.py) before trusting a gain.
cd "$(dirname "$0")/../.."
B=build/bench/bench
[ -x "$1" ] && { B=$1; shift; }
for w in "$@"; do
    base=$(/usr/bin/time -l "$B" cost "$w" 0 2>&1 >/dev/null | awk '/instructions retired/ {print $1}')
    full=$(/usr/bin/time -l "$B" cost "$w" 10 2>&1 >/dev/null | awk '/instructions retired/ {print $1}')
    echo "$w: $(( (full - base) / (10 * 44100) ))"
done
