#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# Build the DSP bench (host only) into build/bench/bench: scalar like the FM-1 (no vectorising, no fused
# multiply-add), so its instruction counts stand in for the device's.
#   tools/bench/build.sh && build/bench/bench render renders/
#   tools/bench/cost.sh                   instructions per sample of each part
set -e
cd "$(dirname "$0")/../.."
mkdir -p build/bench
${CC:-cc} -O2 -fno-vectorize -fno-slp-vectorize -ffp-contract=off -std=c99 -Wall -Wextra -Wno-unused-function \
    -DPLAT_HOST -Ibase/include -Iapp/dsp -o build/bench/bench tools/bench/bench.c app/dsp/*.c -lm
echo "bench: build/bench/bench"
