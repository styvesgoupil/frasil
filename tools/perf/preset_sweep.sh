#!/bin/bash
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# Each drone preset's load in the simulator (render % of real time, best of two 30 s runs): three latched drones, the
# preset turned on; TAPE=1 with LOFI TAPE on. Relative only (the host is some 120 times the FM-1).
#   ./build.sh --host && tools/perf/preset_sweep.sh [TAPE]
cd "$(dirname "$0")/../.."
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
for k in $(seq 0 24); do
    { echo "wait 1500"; echo "tap HOME"; echo "tapkey 5"; echo "tapkey 9"; echo "tapkey 12"; echo "wait 300"
      if [ "${1:-0}" = 1 ]; then for i in 1 2 3 4 5 6; do echo "tap FX"; done; echo "turn K1 1"; echo "tap HOME"; fi
      echo "turn PRESET $k"; echo "wait 30000"; } > "$W/p.txt"
    best=9
    for rep in 1 2; do
        r=$(app/build/host/frasil_host "$W/p.txt" "$W" 2>&1 | grep -o "render [0-9.]*%" | grep -o "[0-9.]*")
        best=$(python3 -c "print(min($best, $r))")
    done
    printf "%s:%s " "$k" "$best"
done
echo
