#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""Objective checks of the bench's renders (tools/bench/bench render DIR): what ears would hear first.

  analyze.py ir DIR/01-impulse-hall.wav      decay times (broadband and per octave), echo density, the tail's
                                             spectral roughness (metallic modes), stereo correlation; a PNG beside it
  analyze.py env FILE.wav ...                the RMS envelope (dB, every 0.5 s): runaway, holds, fades
  analyze.py spec FILE.wav ...               a spectrogram PNG beside each file
"""
import sys
import numpy as np
from scipy.io import wavfile
from scipy import signal
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

FS = 44100


def load(path):
    fs, x = wavfile.read(path)
    assert fs == FS, fs
    if x.dtype == np.int32:      # 24-bit comes in as int32 (left-justified)
        x = x.astype(np.float64) / 2**31
    elif x.dtype == np.int16:
        x = x.astype(np.float64) / 2**15
    return x.astype(np.float64)


def edc_db(h):
    e = np.cumsum((h ** 2)[::-1])[::-1]
    return 10 * np.log10(e / e[0] + 1e-30)


def t60(h, lo=-5.0, hi=-35.0):
    d = edc_db(h)
    i0 = np.argmax(d <= lo)
    i1 = np.argmax(d <= hi)
    if i1 <= i0:
        return float("nan")
    t = np.arange(i0, i1) / FS
    slope = np.polyfit(t, d[i0:i1], 1)[0]
    return -60.0 / slope


def echo_density(h, win=0.02):
    """Abel & Huang's normalised echo density: 1 when the tail is Gaussian (fully diffuse)"""
    n = int(win * FS)
    out = []
    for i in range(0, len(h) - n, n // 2):
        seg = h[i:i + n]
        w = np.hanning(n)
        w /= w.sum()
        sd = np.sqrt(np.sum(w * seg ** 2))
        if sd <= 0:
            out.append(0.0)
            continue
        frac = np.sum(w * (np.abs(seg) > sd))
        out.append(frac / 0.3173)
    return np.array(out)


def roughness(x):
    """the tail's spectrum against its own 1/3-octave smoothing: the deviation in dB of the peaks (metallic modes)"""
    f, p = signal.welch(x, FS, nperseg=8192)
    p = 10 * np.log10(p + 1e-30)
    sm = np.copy(p)
    for i, fc in enumerate(f):
        if fc < 50:
            continue
        lo, hi = fc / 2 ** (1 / 6), fc * 2 ** (1 / 6)
        m = (f >= lo) & (f <= hi)
        sm[i] = 10 * np.log10(np.mean(10 ** (p[m] / 10)))
    band = (f > 150) & (f < 8000)
    dev = p[band] - sm[band]
    return np.percentile(dev, 99), np.std(dev)


def octave_t60(h):
    out = {}
    for fc in (125, 250, 500, 1000, 2000, 4000, 8000):
        sos = signal.butter(4, [fc / np.sqrt(2), min(fc * np.sqrt(2), FS / 2 - 100)], "bandpass", fs=FS, output="sos")
        out[fc] = t60(signal.sosfilt(sos, h))
    return out


def ir(path):
    x = load(path)
    h = x[:, 0] + x[:, 1]
    print(f"{path}")
    print(f"  T60 broadband  {t60(h):6.2f} s   (L {t60(x[:, 0]):.2f}, R {t60(x[:, 1]):.2f})")
    print("  T60 octaves    " + "  ".join(f"{fc}:{v:.2f}" for fc, v in octave_t60(h).items()))
    ned = echo_density(x[:, 0])
    t_ned = np.argmax(ned > 0.9) * 0.01
    print(f"  echo density   reaches 0.9 at {t_ned * 1000:.0f} ms; mean 0.3-1 s {np.mean(ned[30:100]):.2f}")
    tail = x[int(0.5 * FS):int(3.0 * FS)]
    p99, sd = roughness(tail[:, 0])
    print(f"  tail spectrum  99th pct peak over smooth {p99:.1f} dB, std {sd:.2f} dB (white noise: ~6.5 / ~2.6)")
    c = np.corrcoef(tail[:, 0], tail[:, 1])[0, 1]
    print(f"  L/R corr tail  {c:+.3f}")
    fig, ax = plt.subplots(3, 1, figsize=(10, 10))
    t = np.arange(len(h)) / FS
    ax[0].plot(t, edc_db(h))
    ax[0].set_ylim(-80, 0)
    ax[0].set_title("energy decay (dB)")
    ax[1].plot(np.arange(len(ned)) * 0.01, ned)
    ax[1].set_xlim(0, 1.0)
    ax[1].set_title("echo density (1 = diffuse)")
    f, ps = signal.welch(tail[:, 0], FS, nperseg=8192)
    ax[2].semilogx(f, 10 * np.log10(ps + 1e-30))
    ax[2].set_xlim(50, 20000)
    ax[2].set_title("tail spectrum 0.5-3 s")
    fig.tight_layout()
    fig.savefig(path.replace(".wav", ".png"), dpi=80)


def env(path):
    x = load(path)
    m = (x ** 2).mean(axis=1)
    n = FS // 2
    vals = [10 * np.log10(m[i:i + n].mean() + 1e-20) for i in range(0, len(m) - n, n)]
    print(f"{path.split('/')[-1]:28s} " + " ".join(f"{v:4.0f}" for v in vals))


def spec(path):
    x = load(path)
    m = x.mean(axis=1)
    f, t, s = signal.spectrogram(m, FS, nperseg=4096, noverlap=3072)
    fig, ax = plt.subplots(figsize=(11, 5))
    ax.pcolormesh(t, f, 10 * np.log10(s + 1e-14), vmin=-120, vmax=-30, shading="auto", cmap="magma")
    ax.set_yscale("symlog", linthresh=200)
    ax.set_ylim(30, 16000)
    ax.set_title(path.split("/")[-1])
    fig.tight_layout()
    fig.savefig(path.replace(".wav", "-spec.png"), dpi=70)


if __name__ == "__main__":
    what, files = sys.argv[1], sys.argv[2:]
    for p in files:
        {"ir": ir, "env": env, "spec": spec}[what](p)
