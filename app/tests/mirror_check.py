#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""The screen mirror's picture (card #279): a stream the simulator recorded (`mirror out:NAME.syx`, ended at the
screen's dump), rebuilt as a viewer does (fm1-base's tools/fm1_mirror.py Screen, the decoder of its web/mirror page),
against the screen itself (`fbdump NAME.raw`, 240 x 240 RGB565, high byte first).

    python3 app/tests/mirror_check.py [--png] OUTDIR SPEC [SPEC...]
      NAME                 NAME.syx against NAME.raw: the whole screen, pixel for pixel
      NAME:Y0-Y1[,Y0-Y1]   those rows alone (inclusive; the river moving in between)
      NAME:decodes         every frame readable, some screen in it (a screen that moves all over: the CORNER!)
      NAME:covers          and every row of the screen sent at least once (the logo's, alive, on a slow link)
    --png                  NAME-mirror.png too, the viewer's picture (to look at)

A line a SPEC: "  ok mirror NAME ..." or "FAIL mirror NAME ..." (the rows that differ); exit 1 on a FAIL.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "base", "tools"))
import fm1_mirror as M  # noqa: E402

W = H = 240


def runs(rows):
    """[3, 4, 5, 9] -> "3-5, 9" """
    out, i = [], 0
    while i < len(rows):
        j = i
        while j + 1 < len(rows) and rows[j + 1] == rows[j] + 1:
            j += 1
        out.append(str(rows[i]) if i == j else f"{rows[i]}-{rows[j]}")
        i = j + 1
    return ", ".join(out)


def check(out, spec, png=False):
    name, _, what = spec.partition(":")
    stream = open(os.path.join(out, name + ".syx"), "rb").read()
    scr, cmds, sent = M.Screen(), {}, set()
    for pkt in M.split(stream):
        c = scr.frame(pkt)
        cmds[c] = cmds.get(c, 0) + 1
        f = M.B.parse_frame(pkt) if c in (M.MR_RECT, M.MR_FILL) else None
        if f:
            sent.update(range(f[1][1], min(H, f[1][1] + f[1][3])))
    if png:
        scr.png(os.path.join(out, name + "-mirror.png"))
    told = (f"{len(stream)} B, {cmds.get(M.MR_RECT, 0)} rects, {cmds.get(M.MR_FILL, 0)} fills, "
            f"{cmds.get(M.MR_REQ, 0)} hellos, {cmds.get(M.MR_INPUT, 0)} input reports")
    if scr.bad:
        print(f"FAIL mirror {name}: {scr.bad} frames a viewer cannot read ({told})")
        return False
    if what in ("decodes", "covers"):
        if not cmds.get(M.MR_RECT, 0):
            print(f"FAIL mirror {name}: no screen in it ({told})")
            return False
        never = [y for y in range(H) if y not in sent]
        if what == "covers" and never:
            print(f"FAIL mirror {name}: rows {runs(never)} never sent ({told})")
            return False
        print(f"  ok mirror {name}: every frame readable{', every row sent' if what == 'covers' else ''} ({told})")
        return True
    raw = open(os.path.join(out, name + ".raw"), "rb").read()
    if len(raw) != W * H * 2:
        print(f"FAIL mirror {name}: {name}.raw is {len(raw)} B, not {W * H * 2}")
        return False
    rows = range(H)
    if what:
        rows = [y for part in what.split(",") for y in range(int(part.split("-")[0]), int(part.split("-")[1]) + 1)]
    bad = [y for y in rows if any(scr.px[y * W + x] != (raw[2 * (y * W + x)] << 8 | raw[2 * (y * W + x) + 1])
                                  for x in range(W))]
    which = f" (rows {what})" if what else ""
    if bad:
        print(f"FAIL mirror {name}{which}: rows {runs(bad)} differ from the screen ({told})")
        return False
    print(f"  ok mirror {name}{which}: the screen, pixel for pixel ({told})")
    return True


def main(argv):
    png = bool(argv) and argv[0] == "--png"
    argv = argv[1:] if png else argv
    if len(argv) < 2:
        print(__doc__)
        return 2
    ok = True
    for spec in argv[1:]:
        ok = check(argv[0], spec, png) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
