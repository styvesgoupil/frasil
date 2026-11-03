# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""ENERGY from Python (FRASIL STREAM's service): engine/tools/energy (energy.c) run on a piece's bytes, its JSON back.

    from energy import measure
    m = measure(blob)                          # a shard (a River scene's bytes), 3:00 on air
    m = measure(blob, length_ms=240000)        # a shard fitted to 4:00
    m = measure(frstream, in_ms=12000, out_ms=580000)   # a stream's part on air
    m["energy"], m["bpm"], m                   # 0 .. 1000; the tempo; the counts behind it (stream_pieces.measure)

The tool: ENERGY_TOOL (env), else engine/tools/energy beside this file's repo build (build/engine/energy), else `energy`
on PATH. Raises EnergyError (its .code: the tool's exit, 1 arguments, 2 not a piece, 3 another layout, 4 no engine)."""
import json
import os
import shutil
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
CALIBRATION = HERE / "energy-calibration.json"


class EnergyError(Exception):
    def __init__(self, code, msg):
        super().__init__(msg)
        self.code = code


def tool():
    for p in (os.environ.get("ENERGY_TOOL"), HERE.parents[1] / "build" / "engine" / "energy", shutil.which("energy")):
        if p and Path(p).is_file() and os.access(p, os.X_OK):
            return str(p)
    raise EnergyError(127, "no energy tool (engine/tools/build.sh, or ENERGY_TOOL)")


def calibration(path=CALIBRATION):
    j = json.loads(Path(path).read_text())
    return j["r0"], j["r98"]


def measure(blob, length_ms=None, in_ms=None, out_ms=None, r0=None, r98=None, weights=None, timeout=30):
    args = [tool()]
    if length_ms is not None:
        args += ["--len", str(int(length_ms))]
    if in_ms is not None:
        args += ["--in", str(int(in_ms))]
    if out_ms is not None:
        args += ["--out", str(int(out_ms))]
    if r0 is not None:
        args += ["--r0", repr(float(r0)), "--r98", repr(float(r98))]
    if weights is not None:
        args += ["--w", ",".join(repr(float(w)) for w in weights)]
    r = subprocess.run(args + ["-"], input=bytes(blob), capture_output=True, timeout=timeout)
    if r.returncode:
        raise EnergyError(r.returncode, r.stderr.decode("utf-8", "replace").strip())
    return json.loads(r.stdout)
