#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""The screen mirror's remote (card #279): a viewer's buttons and knobs working FRASIL's (fm1-base's cmd 0x54, held as
the panel's: plat_buttons, plat_enc), on the simulator driven as an FM-1 over USB (--serve). The mirror asked on; FX
pressed and let go from the viewer: FRASIL's FX page, HOME back; SELECT turned: the next track and back; FX held, the
viewer gone (mirror off): let go, a tap. Each request answered done; nothing from the viewer before the mirror is on.
(Its keys: fm1-base's touch.c takes the keys with velocity from their readings alone, so a viewer's do not play them.)

    python3 app/tests/mirror_remote.py OUTDIR        (after ./build.sh --host)
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
sys.path.insert(0, os.path.join(ROOT, "base", "tools"))
import fm1_backup as B  # noqa: E402

MR_REQ, MR_REMOTE = 0x50, 0x54
B_FX, B_HOME, EN_SELECT = 0, 6, 0             # (plat.h's B_*, EN_*)


class Dev:
    def __init__(self, out):
        self.p = subprocess.Popen([os.path.join(ROOT, "app/build/host/frasil_host"), "--serve", out],
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
        self.fails = 0

    def say(self, line):
        self.p.stdin.write(line + "\n")
        self.p.stdin.flush()

    def run(self, ms):
        """ms of the device: the frames it sent meanwhile"""
        self.say(f"run {ms}")
        got = []
        for ln in self.p.stdout:
            if ln.startswith("ok "):
                return got
            if ln.startswith("sx "):
                f = B.parse_frame(bytes.fromhex(ln[3:].strip()))
                if f:
                    got.append(f)
        raise SystemExit("FAIL mirror remote: the simulator stopped")

    def sx(self, cmd, body):
        self.say("sx " + B.frame(cmd, bytes(body)).hex())

    def expect(self, what, value, why):
        self.say(f"expect {what} {value}")
        ln = self.p.stdout.readline().rstrip()
        ok = ln.lstrip().startswith("ok")
        print(f"  ok mirror remote: {why} ({ln.strip()})" if ok else f"FAIL mirror remote: {why} ({ln.strip()})")
        self.fails += not ok

    def remote(self, seq, op, a, b, why, want=1):
        self.sx(MR_REMOTE, [seq, op, a, b, 0])
        ans = [b for c, b in self.run(30) if c == MR_REMOTE]
        ok = bool(ans) and list(ans[-1][:3]) == [seq, op, want]
        print(f"  ok mirror remote: {why}" if ok else f"FAIL mirror remote: {why} (answer {[list(x) for x in ans]})")
        self.fails += not ok


def main(out):
    os.makedirs(out, exist_ok=True)
    d = Dev(out)
    d.run(6000)                           # (past the logo)
    d.remote(1, 1, B_FX, 1, "refused before the mirror is on", want=0)
    d.sx(MR_REQ, [1])
    hello = [b for c, b in d.run(100) if c == MR_REQ]
    ok = bool(hello) and hello[-1][1] == 1
    print("  ok mirror remote: the mirror on, its hello" if ok else f"FAIL mirror remote: no hello ({hello})")
    d.fails += not ok
    d.expect("page", 0, "the play page")
    d.remote(2, 1, B_FX, 1, "FX down from the viewer")
    d.run(60)
    d.remote(3, 1, B_FX, 0, "FX up")
    d.run(300)
    d.expect("page", 29, "FX's page (GENERATIVE FX), as the panel's FX")
    d.remote(4, 1, B_HOME, 1, "HOME down")
    d.run(60)
    d.remote(5, 1, B_HOME, 0, "HOME up")
    d.run(300)
    d.expect("page", 0, "back on the play page")
    d.remote(6, 3, EN_SELECT, 1, "SELECT turned a detent from the viewer")
    d.run(300)
    d.expect("p166", 1, "the keys on GLINT")
    d.remote(7, 3, EN_SELECT, 0xFF, "and back")
    d.run(300)
    d.expect("p166", 0, "the keys on DRONE")
    d.remote(8, 1, B_FX, 1, "FX held from the viewer")
    d.run(100)
    d.sx(MR_REQ, [0])
    d.run(400)
    d.expect("page", 29, "the viewer gone: FX let go (a tap), never left held")
    d.say("quit")
    d.p.wait(10)
    return 1 if d.fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]) if len(sys.argv) > 1 else __doc__)
