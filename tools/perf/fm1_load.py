#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""The render's load on a connected FM-1, measured as the user plays it (fm1-base's log: "audio <load %> max <us>",
"of <budget us> late <halves>"; the longest and the late reset at each read). Needs mido and sounddevice (.venv).

  tools/perf/fm1_load.py read [SECONDS]      the load over SECONDS (default 6): average %, longest render, late halves
  tools/perf/fm1_load.py quiet               SPK/JACK OFF, checked (do it first, every session: the user sleeps)
  tools/perf/fm1_load.py probe [--tag T]     the engine's cost under the shedding's threshold (builds compare): at
                                             rest, four drone keys held, the same on RUST; a voice's share
  tools/perf/fm1_load.py profile [--tag T]   where the render's time goes (b21 on), at rest, four keys, on RUST
  tools/perf/fm1_load.py scenarios [--tag T] the standard heavy cases on the user's FM-1, then its state put back:
      S1 the scene playing (PLAY from stopped), no effect
      S2 + LOFI TAPE on (its TRACKS as they are)
      S3 + the drone preset RUST (the heaviest: CHOIR SHAPE and RESONANCE, GRAIN, BEAT, AIR)
      S4 S3 while the USB audio records 20 s (the user's case: recording into an iPhone)
    SPK/JACK is set OFF first and LEFT OFF (the user sleeps beside it: no sound in the room, ever; --speaker-back to
    put it back as it was, only when the user asks). The FM-1's state before is read from a fresh backup
    (build/backups/perf-*.frasil): TAPE and the drone's PRESET go back to it; the playing stops (PLAY tapped twice:
    RELEASE ALL). Start it with the FM-1 stopped, at rest.

"Smooth" (the optimisation plan, .omc/plans/frasil-engine-optimization.md): S4 with 0 late halves over 20 s, the
longest render under 92 % of its time, without the shedding past its first step (the amber dot does not stay). From
2.5-b18 each line has the shedding's step at its end ("shed N (top M)": now, and the highest meanwhile)."""
import json
import re
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PY = str(ROOT / ".venv" / "bin" / "python")
REMOTE = [PY, str(ROOT / "base" / "tools" / "fm1_remote.py")]
SCHEMA = json.loads((ROOT / "server" / "schema.json").read_text())
IDS = SCHEMA["ids"]


def rc(*cmds):
    """fm1_remote commands, in order (tap BTN, turn ENC N, wait MS ...)"""
    subprocess.run(REMOTE + [str(c) for c in cmds], capture_output=True, text=True)


def log():
    """(load %, longest us, budget us, late halves, the app's word or None) since the last read, or None. FRASIL's
    word (2.5-b18 on): the shedding's step now (its low byte), the highest since the last read (the next), and from
    b19 bit 16: anything playing (held, latched, a sequence running)"""
    out = subprocess.run(REMOTE + ["log"], capture_output=True, text=True).stdout
    m = re.search(r"audio ([0-9A-F]{8}) max ([0-9A-F]{8})\s*\n.*?of ([0-9A-F]{8}) late ([0-9A-F]{8})(?: app ([0-9A-F]{8}))?",
                  out)
    return tuple(int(x, 16) if x else None for x in m.groups()) if m else None


def measure(tag, secs=6.0, during=None):
    time.sleep(1.5)
    log()                                          # (settled; the longest and the late cleared)
    if during:
        during()
    else:
        time.sleep(secs)
    a = log()
    if not a:
        print(f"{tag:28s} no reading (an fm1-base without the audio log?)")
        return None
    shed = "" if a[4] is None else f"  shed {a[4] & 255} (top {a[4] >> 8 & 255})"
    print(f"{tag:28s} load {a[0]:3d} %  longest {a[1]:5d} us of {a[2]} ({100 * a[1] / a[2]:.0f} %)  late {a[3]}{shed}",
          flush=True)
    return a


def record(secs, out):
    """the FM-1's USB audio (the input named FRASIL) into a 16-bit WAV, PortAudio's way (ffmpeg drops frames)"""
    import numpy as np
    import sounddevice as sd
    import wave
    dev = next(i for i, d in enumerate(sd.query_devices()) if "FRASIL" in d["name"] and d["max_input_channels"] >= 2)
    chunks = []
    with sd.InputStream(device=dev, channels=2, samplerate=44100, dtype="int16", latency="high",
                        callback=lambda d, f, t, s: chunks.append(d.copy())):
        sd.sleep(int(secs * 1000))
    x = np.concatenate(chunks)
    with wave.open(str(out), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(44100)
        w.writeframes(x.tobytes())


def state():
    """the FM-1's settings as stored (a fresh backup): TAPE, the filter's TYPE, the EQ's PRESET, the drone's PRESET,
    SPK/JACK (the sound loaded at power-on is the settings' record)"""
    sys.path.insert(0, str(ROOT / "base" / "tools"))
    import fm1_backup as fb
    out = ROOT / "build" / "backups" / time.strftime("perf-%Y%m%d-%H%M%S.frasil")
    subprocess.run([PY, str(ROOT / "base" / "tools" / "fm1_backup.py"), "backup", str(out), "--magic", "FRBK"],
                   capture_output=True, text=True)
    _, objs = fb.parse(out.read_bytes())
    st = SCHEMA["settings_t"]
    for _, data in objs:
        if data[:4] == SCHEMA["magic"]["settings"].encode():
            v = data[st["v"]:]
            return {"tape": v[IDS["P_TP_ON"]], "filter": v[IDS["P_FL_TYPE"]], "eq": v[IDS["P_EQ_PRESET"]],
                    "drone": v[IDS["P_DR_PRESET"]], "speaker": not data[st["speaker_off"]], "backup": str(out)}
    raise SystemExit("fm1_load: no settings record of this layout in the backup (flash the current build first)")


GLO6 = ["tap", "GLO", "wait", 150] * 6           # (from another button's page: GLOBAL, MIDI, MASTERING, EQ, EQ II,
FX6 = ["tap", "FX", "wait", 150] * 6             #  ROUTING; FX: GENERATIVE, CLOUD I-III, FILTER, TAPE)
RUST = 23                                        # (PRESET's value: 0 CUSTOM, 1 MIST ... 23 RUST, 24 STILL)


def quiet():
    """MASTER at 0 (remote: till "master pot" or a restart; the speaker's and the jack's level, not the USB audio's)
    and SPK/JACK OFF (ROUTING's K4), checked from a backup: the speaker and the jack quiet, the USB audio on. (2026-10-30:
    renders running late crackled in the speaker with SPK/JACK OFF: both, and the shedding never disabled)"""
    rc("master", 0, "wait", 100)
    rc("tap", "HOME", "wait", 200, *GLO6, "wait", 200, "turn", "K4", -1, "wait", 300, "tap", "HOME", "wait", 300)
    s = state()
    if s["speaker"]:
        raise SystemExit("fm1_load: SPK/JACK still ON after turning it off: stop, do not play anything")
    return s


def stop():
    """the playing stopped (PLAY twice quick: RELEASE ALL), checked with the log's word where it has it (b19 on):
    else PLAY held (RELEASE ALL while anything plays), checked again; still playing: stop here"""
    rc("tap", "PLAY", "wait", 120, "tap", "PLAY", "wait", 600, "tap", "HOME", "wait", 200)
    for _ in range(3):
        a = log()
        if not a or a[4] is None or not a[4] >> 16 & 1:
            return
        rc("press", "PLAY", "wait", 900, "release", "PLAY", "wait", 600)
    raise SystemExit("fm1_load: the FM-1 still plays after RELEASE ALL: stop, look at it")


def probe(tag, keys=(5, 9, 12, 16)):
    """the engine's cost under the shedding's threshold (82 %: no step taken, so builds compare): P0 at rest, P1 four
    drone keys held (the scene's sound), P2 the same on RUST; each a voice's share ((P - P0) / keys) too"""
    s0 = state()
    quiet()
    rc("wait", 4000)
    r = measure(f"{tag} P0 rest")
    downs = [c for k in keys for c in ("key", k, "down")]
    ups = [c for k in keys for c in ("key", k, "up")]
    rc(*downs, "wait", 2500)
    a = measure(f"{tag} P1 {len(keys)} keys")
    rc("turn", "PRESET", -40, "wait", 200, "turn", "PRESET", RUST, "wait", 2500)
    b = measure(f"{tag} P2 {len(keys)} keys RUST")
    rc(*ups, "wait", 300, "turn", "PRESET", -40, "wait", 200, "turn", "PRESET", s0["drone"], "wait", 300)
    if r and a and b:
        print(f"{tag} a voice: scene {(a[0] - r[0]) / len(keys):.1f} %, RUST {(b[0] - r[0]) / len(keys):.1f} %  "
              f"(rest {r[0]} %; shed top {max(a[4] >> 8 & 255, b[4] >> 8 & 255) if a[4] is not None else '?'})")
    rc("wait", 3000)
    stop()


PARTS = ["setup", "voice blocks", "CHOIR", "PARTIALS", "BREATH", "voices' mix", "warmth", "cloud", "mix", "GLINT", "BASS"]


def parts(secs=4.0):
    """each part of the render's share of its time (per mille), over secs (b21 on: the log's word, a part a read)"""
    for _ in PARTS:
        log()                                      # (each part's count from now)
    time.sleep(secs)
    out = {}
    for _ in range(len(PARTS) + 2):
        a = log()
        if a and a[4] is not None:
            out[a[4] >> 17 & 15] = (a[4] >> 21) / 10.0
    return out


def profile(tag, keys=(5, 9, 12, 16)):
    """where the render's time goes on the FM-1: at rest, then four drone keys held (the scene's sound; under the
    shedding's threshold), then the same on RUST (shed as it must)"""
    s0 = state()
    quiet()
    downs = [c for k in keys for c in ("key", k, "down")]
    ups = [c for k in keys for c in ("key", k, "up")]
    rows = []
    rc("wait", 3000)
    rows.append(("rest", parts(), log()))
    rc(*downs, "wait", 2500)
    rows.append((f"{len(keys)} keys", parts(), log()))
    rc("turn", "PRESET", -40, "wait", 200, "turn", "PRESET", RUST, "wait", 2500)
    rows.append((f"{len(keys)} keys RUST", parts(), log()))
    rc(*ups, "wait", 300, "turn", "PRESET", -40, "wait", 200, "turn", "PRESET", s0["drone"], "wait", 300)
    print(f"{tag:10s} " + " ".join(f"{p[:8]:>8s}" for p in PARTS) + "     sum   load shed")
    for name, pt, a in rows:
        print(f"{name:10s} " + " ".join(f"{pt.get(k, float('nan')):7.1f}%" for k in range(len(PARTS))) +
              f" {sum(pt.values()):6.1f}%  {a[0] if a else '?':>3} %  {a[4] & 255 if a and a[4] is not None else '?'}")
    rc("wait", 3000)
    stop()


def scenarios(tag, speaker_back=False):
    s0 = state()
    print(f"before: {s0}")
    quiet()
    rc("tap", "PLAY", "wait", 1500)
    measure(f"{tag} S1 scene")
    rc(*FX6, "wait", 200, "turn", "K1", 1, "wait", 300)
    measure(f"{tag} S2 + TAPE")
    rc("turn", "PRESET", -40, "wait", 200, "turn", "PRESET", RUST, "wait", 1500)
    measure(f"{tag} S3 + RUST (20 s)", 20.0)
    measure(f"{tag} S4 + USB recording", during=lambda: record(20, ROOT / "build" / "perf-s4.wav"))
    rc("turn", "PRESET", -40, "wait", 200, "turn", "PRESET", s0["drone"], "wait", 300)   # (CUSTOM back: its own)
    if not s0["tape"]:
        rc("turn", "K1", -1, "wait", 300)
    stop()
    if speaker_back and s0["speaker"]:
        rc(*GLO6, "wait", 200, "turn", "K4", 1, "wait", 300, "tap", "HOME", "wait", 200)
    print("after: the drone's PRESET and TAPE as they were; stopped; SPK/JACK " +
          ("as it was" if speaker_back else "OFF (left so)"))


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "read":
        measure("now", float(sys.argv[2]) if len(sys.argv) > 2 else 6.0)
    elif len(sys.argv) > 1 and sys.argv[1] == "quiet":
        print(quiet())
    elif len(sys.argv) > 1 and sys.argv[1] == "profile":
        a = sys.argv[2:]
        profile(a[a.index("--tag") + 1] if "--tag" in a else "")
    elif len(sys.argv) > 1 and sys.argv[1] == "probe":
        a = sys.argv[2:]
        probe(a[a.index("--tag") + 1] if "--tag" in a else "")
    elif len(sys.argv) > 1 and sys.argv[1] == "scenarios":
        a = sys.argv[2:]
        scenarios(a[a.index("--tag") + 1] if "--tag" in a else "", "--speaker-back" in a)
    else:
        print(__doc__)
