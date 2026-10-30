# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# Perform's cost on the FM-1: the render's load (fm1-base's log) with the scene playing, Perform off, then on (a page's
# HELLO every 2 s, its frames read), twice each, all through one MIDI connection (two programs on the FM-1's port at
# once hung fm1_remote's sends). The scene is started (PLAY) and stopped (PLAY twice: RELEASE ALL) by the script.
#   .venv/bin/python design/perform/scripts/fm1_load_ab.py [SECONDS]
import re
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "base" / "tools"))
import fm1_remote as R  # noqa: E402
import mido  # noqa: E402

FP = [0x7D, 0x46, 0x50]
secs = float(sys.argv[1]) if len(sys.argv) > 1 else 12.0
fm = R.FM1()
frames = 0


def drain(perform):
    global frames
    for m in fm.inp.iter_pending():
        if perform and m.type == "sysex" and list(m.data[:3]) == FP and m.data[3] == 6:
            frames += 1


def tap(b):
    fm.btn |= 1 << R.BTN.index(b)
    fm.buttons()
    time.sleep(0.1)
    fm.btn &= ~(1 << R.BTN.index(b))
    fm.buttons()
    time.sleep(0.05)


def log():
    fm.sysex(R.LG)
    text = "\n".join(f.decode("latin1", "replace") for f in fm.frames(R.LG, 2.0, until=lambda f: f == b"end"))
    m = re.search(r"audio ([0-9A-F]{8}) max ([0-9A-F]{8}).*?of ([0-9A-F]{8}) late ([0-9A-F]{8})(?: app ([0-9A-F]{8}))?", text, re.S)
    return tuple(int(x, 16) if x else 0 for x in m.groups()) if m else None


def run(tag, perform):
    global frames
    t0, last = time.time(), 0.0
    frames = 0
    while time.time() - t0 < 1.5 + secs:
        if perform and time.time() - last > 2:
            fm.sysex(FP + [1, 1])
            last = time.time()
        if last and abs(time.time() - t0 - 1.5) < 0.02:
            pass
        drain(perform)
        time.sleep(0.01)
        if time.time() - t0 >= 1.5 and not getattr(run, "cleared", False):
            log()                                  # (settled: the longest and the late cleared)
            run.cleared = True
            frames = 0
    run.cleared = False
    a = log()
    if not a:
        print(f"{tag:14s} no reading")
        return
    print(f"{tag:14s} load {a[0]:3d} %  longest {a[1]:5d} us of {a[2]} ({100 * a[1] / a[2]:.0f} %)  late {a[3]}  "
          f"shed {a[4] & 255} (top {a[4] >> 8 & 255})" + (f"  frames {frames / secs:.1f}/s" if perform else ""), flush=True)


tap("PLAY")
time.sleep(3)
for k in (1, 2):
    run(f"off #{k}", False)
    run(f"PERFORM #{k}", True)
    fm.sysex(FP + [2])
    time.sleep(0.5)
tap("PLAY")
time.sleep(0.15)
tap("PLAY")                                        # (twice quickly: RELEASE ALL)
drain(False)
print("stopped")
