# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""FRASIL's logo as if printed on the device: its outlines traced from app/assets/frasil-logo.png (the letters above
the waterline with the blade through them, the letters under it), two flat inks, no glow, no fade. Writes
website/assets/img/logo-print.svg, which the site's drawings of the FM-1 put under its screen (the guide's panel, the
play page's).
    .venv/bin/python tools/logo_print.py
The cut is website/assemble.py's (LOGO_BOX, LOGO_SPLIT; its logo_layers' thresholds), the inks its FOG and SEA."""
from pathlib import Path

import numpy as np
from contourpy import contour_generator
from PIL import Image

REPO = Path(__file__).resolve().parent.parent
LOGO_BOX = (216, 531, 2540, 1003)        # (website/assemble.py's)
LOGO_SPLIT = 768
FOG, SEA = "#dce3ea", "#3480ec"
TOL = 0.9                                # how far a traced point may stray from the outline, source pixels


def simplify(pts, tol):
    """Douglas-Peucker on a closed outline (its first point kept)"""
    keep = np.zeros(len(pts), bool)
    keep[0] = keep[-1] = True
    stack = [(0, len(pts) - 1)]
    while stack:
        a, b = stack.pop()
        if b <= a + 1:
            continue
        p, q = pts[a], pts[b]
        d = q - p
        n = np.hypot(*d)
        seg = pts[a + 1:b]
        dist = np.abs(d[0] * (seg[:, 1] - p[1]) - d[1] * (seg[:, 0] - p[0])) / n if n else np.hypot(*(seg - p).T)
        i = int(np.argmax(dist))
        if dist[i] > tol:
            keep[a + 1 + i] = True
            stack += [(a, a + 1 + i), (a + 1 + i, b)]
    return pts[keep]


def outline(mask):
    """a mask's outlines at half its alpha -> SVG path data (even-odd: the counters are holes)"""
    z = np.pad(mask, 1).astype(float)
    out = []
    for line in contour_generator(z=z).lines(0.5):
        if len(line) < 8:
            continue
        pts = simplify(line - 1.0, TOL)
        if len(pts) < 4:
            continue
        out.append("M" + "L".join(f"{x:.1f} {y:.1f}" for x, y in pts[:-1]) + "Z")
    return "".join(out)


def main():
    src = Image.open(REPO / "app" / "assets" / "frasil-logo.png").convert("L").crop(LOGO_BOX)
    lum = np.asarray(src, float)
    w, h = src.size
    split = LOGO_SPLIT - LOGO_BOX[1]
    top = np.clip((lum - 24) / (220 - 24), 0, 1)
    low = np.clip((lum - 24) / (194 - 24), 0, 1)
    top[split:] = 0
    low[:split] = 0
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}">'
           f'<path fill="{FOG}" fill-rule="evenodd" d="{outline(top)}"/>'
           f'<path fill="{SEA}" fill-rule="evenodd" d="{outline(low)}"/></svg>\n')
    out = REPO / "website" / "assets" / "img" / "logo-print.svg"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(svg, encoding="utf-8")
    print(f"logo-print {out.relative_to(REPO)}  {w} x {h}, {len(svg)} bytes")


if __name__ == "__main__":
    main()
