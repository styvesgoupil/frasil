#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""The same script rendered at two rates (engine/tests/run.sh: 44.1 and 48 kHz), second by second: its level (dB,
the sides summed), its zero crossings a second, its brightness (the frequency its first difference says, Hz). The same
sound at another rate: levels within a dB, crossings and brightness within a few per cent, but where it moves
(a change on the second's edge, a random step).
    python3 engine/tests/rates.py A.wav B.wav"""
import array
import math
import sys
import wave


def seconds(path):
    w = wave.open(path)
    fs, n = w.getframerate(), w.getnframes()
    a = array.array("h", w.readframes(n))
    out = []
    for s in range(n // fs):
        e = d = 0.0
        zc = 0
        last = a[2 * s * fs] + a[2 * s * fs + 1]
        for i in range(s * fs, (s + 1) * fs):
            m = a[2 * i] + a[2 * i + 1]
            e += m * m
            d += (m - last) * (m - last)
            zc += (m < 0) != (last < 0)
            last = m
        rms = math.sqrt(e / fs) / 65536.0
        out.append((20.0 * math.log10(rms + 1e-9), zc, math.sqrt(d / (e + 1e-9)) * fs / (2.0 * math.pi)))
    return fs, out


fa, a = seconds(sys.argv[1])
fb, b = seconds(sys.argv[2])
n = min(len(a), len(b))
level = sorted(abs(a[i][0] - b[i][0]) for i in range(n) if a[i][0] > -60.0)
zc = sorted(abs(b[i][1] / max(a[i][1], 1) - 1.0) for i in range(n) if a[i][0] > -60.0)
br = sorted(abs(b[i][2] / max(a[i][2], 1e-9) - 1.0) for i in range(n) if a[i][0] > -60.0)


def q(v, f):
    return v[min(len(v) - 1, int(f * len(v)))] if v else 0.0


print(f"rates: {fa} Hz and {fb} Hz, {n} seconds (louder than -60 dB: {len(level)})")
print(f"  level:      median {q(level, 0.5):.2f} dB apart, 90 % within {q(level, 0.9):.2f} dB, at most {q(level, 1.0):.2f}")
print(f"  crossings:  median {100 * q(zc, 0.5):.1f} % apart, 90 % within {100 * q(zc, 0.9):.1f} %")
print(f"  brightness: median {100 * q(br, 0.5):.1f} % apart, 90 % within {100 * q(br, 0.9):.1f} %")
loud = [i for i in range(n) if a[i][0] > -60.0]
zs = sum(b[i][1] / max(a[i][1], 1) - 1.0 for i in loud) / max(len(loud), 1)
bs = sum(b[i][2] / max(a[i][2], 1e-9) - 1.0 for i in loud) / max(len(loud), 1)
ls = sum(b[i][0] - a[i][0] for i in loud) / max(len(loud), 1)
print(f"  on average: {ls:+.2f} dB, crossings {100 * zs:+.1f} %, brightness {100 * bs:+.1f} %")
ok = abs(ls) < 0.5 and abs(zs) < 0.02 and abs(bs) < 0.02
print("  the same sound" if ok else "  NOT the same sound")
sys.exit(0 if ok else 1)
