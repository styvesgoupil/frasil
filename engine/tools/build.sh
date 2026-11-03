#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# engine/tools/energy, ENERGY's measure (energy.c), built as engine/tests/run.sh builds its tests: the engine alone, its
# DSP's FRASIL_EV_LOG hook the tool's own. No more than cc and libm (a Dockerfile's gcc builder stage: the same line).
#   engine/tools/build.sh [OUT_DIR]       (default build/engine: build/engine/energy)
set -e
cd "$(dirname "$0")/../.."
OUT="${1:-build/engine}"
mkdir -p "$OUT"
DSP="engine/dsp/frasil.c engine/dsp/glint.c engine/dsp/bass.c engine/dsp/nuage.c"
CF="-O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wno-unused-function -Iengine -Iengine/dsp"
${CC:-cc} $CF -DFRASIL_EV_LOG=energy_log -o "$OUT/energy" engine/tools/energy.c engine/frasil_stream.c engine/frasil_engine.c $DSP -lm
echo "$OUT/energy"
