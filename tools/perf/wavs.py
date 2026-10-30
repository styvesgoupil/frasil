#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""Comparing the bench's renders (tools/bench/bench fx WHICH OUT.wav), old engine against new.

  tools/perf/wavs.py null OLD.wav NEW.wav     the difference's level against OLD's (dB): -inf identical; under -90 dB
                                              a change nobody hears (rounding); above -60 dB: listen (A/B). Renders
                                              made DRY=1 (the bench without its cloud): the cloud's int16 lines make
                                              any change upstream a reshuffle of their rounding, some -75 dB
  tools/perf/wavs.py steps OLD.wav NEW.wav    a setting changed every 200 ms from 1 s (the bench's fltype, tptracks,
                                              shed ...): the high-frequency burst after each change against the
                                              100 ms before (dB): a click shows as a burst (fixed ones: under ~10 dB)"""
import sys

import numpy as np
import wave


def rd(p, ch=None):
    w = wave.open(p)
    n, sw, nc = w.getnframes(), w.getsampwidth(), w.getnchannels()
    b = np.frombuffer(w.readframes(n), dtype=np.uint8)
    if sw == 3:
        b = b.reshape(-1, 3)
        x = b[:, 0].astype(np.int32) | b[:, 1].astype(np.int32) << 8 | b[:, 2].astype(np.int32) << 16
        x = np.where(x >= 1 << 23, x - (1 << 24), x) / 8388608.0
    else:
        x = np.frombuffer(b.tobytes(), dtype=np.int16) / 32768.0
    x = x.reshape(-1, nc)
    return x if ch is None else x[:, ch]


def null(a, b):
    x, y = rd(a), rd(b)
    n = min(len(x), len(y))
    d = x[:n] - y[:n]
    r = np.sqrt(np.mean(d * d)) / (np.sqrt(np.mean(x[:n] ** 2)) + 1e-12)
    print(f"difference {20 * np.log10(r + 1e-30):.1f} dB, its peak {20 * np.log10(np.abs(d).max() + 1e-30):.1f} dBFS")


def bursts(p, fs=44100):
    e = np.diff(rd(p, 0), 3) ** 2
    out = []
    for k in range(40):
        a = int((1.0 + 0.2 * k) * fs)
        if a + int(0.06 * fs) >= len(e):
            break
        base = np.median([np.mean(e[j:j + 132]) for j in range(a - int(0.1 * fs), a - int(0.01 * fs), 132)]) + 1e-12
        peak = max(np.mean(e[j:j + 132]) for j in range(a - 132, a + int(0.06 * fs), 44))
        out.append(10 * np.log10(peak / base))
    return np.array(out)


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "null":
        null(sys.argv[2], sys.argv[3])
    elif len(sys.argv) == 4 and sys.argv[1] == "steps":
        o, n = bursts(sys.argv[2]), bursts(sys.argv[3])
        print(f"old: max {o.max():5.1f} dB median {np.median(o):5.1f}   new: max {n.max():5.1f} dB median {np.median(n):5.1f}")
    else:
        print(__doc__)
