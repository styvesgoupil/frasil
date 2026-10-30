# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# Perform's stream, heard from a terminal: HELLO to the FM-1 (FRASIL with PERFORM, app.c app_sysex: F0 7D 46 50),
# then for SECONDS what it says, counted by kind, and the first of each decoded; BYE at the end.
#   .venv/bin/python design/perform/scripts/fm1_listen.py [SECONDS]
import sys
import time

import mido

KIND = {1: "HELLO", 2: "SCENE", 3: "VALUES", 4: "VALUE", 5: "PAGE", 6: "FRAME", 7: "BUTTON"}
secs = float(sys.argv[1]) if len(sys.argv) > 1 else 3.0
name = next(n for n in mido.get_output_names() if "FM-1" in n)
out, inp = mido.open_output(name), mido.open_input(next(n for n in mido.get_input_names() if "FM-1" in n))
hello = mido.Message("sysex", data=[0x7D, 0x46, 0x50, 1, 1])
count, first, t0, last = {}, {}, time.time(), 0.0
out.send(hello)
while time.time() - t0 < secs:
    if time.time() - last > 2:
        out.send(hello)
        last = time.time()
    for m in inp.iter_pending():
        d = list(m.data) if m.type == "sysex" else []
        if d[:3] != [0x7D, 0x46, 0x50]:
            continue
        k = KIND.get(d[3], d[3])
        count[k] = count.get(k, 0) + 1
        first.setdefault(k, d[4:])
    time.sleep(0.002)
out.send(mido.Message("sysex", data=[0x7D, 0x46, 0x50, 2]))
print("in %.1f s:" % secs, ", ".join(f"{k} {v}" for k, v in count.items()))
for k, b in first.items():
    if k == "HELLO":
        print("  HELLO ver", b[0], "NPARAM", b[1] << 7 | b[2], "version", bytes(b[3:]).decode())
    elif k == "SCENE":
        print("  SCENE slot", b[0], "dirty", b[1], "name", repr(bytes(b[2:]).decode()))
    elif k == "PAGE":
        print("  PAGE", b[0], "octave", b[1] - 3, "keys", b[2])
    elif k == "FRAME":
        print("  FRAME drones", [(b[2 * i], b[2 * i + 1]) for i in range(8)], "glints", [tuple(b[16 + 3 * i:19 + 3 * i]) for i in range(4)],
              "bass", b[28:30], "lfo", b[30])
    else:
        print(" ", k, b[:12])
