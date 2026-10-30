#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""FRASIL's website, assembled from website/ into build/website/frasil/ (tools/website.sh runs this).

  website/site.toml          the release status, the URLs, the guide's order: what changes lives there
  website/templates/         the page's shell, the header, the footer, the guide's frame
  website/pages/             the pages' own content: index (the landing), 404, play (around the emulator), cheatsheet,
                             river-library (/river/: what the River is, and the way there)
                             (FRASIL on one A4 page), guide/*, river/* (the River: river.frasilsynth.com, its own site
                             in OUT_PARENT/river: navigate, yours, tools, backup, profile, privacy)
  website/admin/             the admin page (admin.frasilsynth.com, OUT_PARENT/admin): accounts, uploads, waitlists
  website/assets/            CSS, JS, the favicon's SVG and the cheat sheet's PDF and PNG (tools/cheatsheet.sh makes
                             them from the assembled page), copied as they are

Made here, from the repository: the logo's layers (app/assets/frasil-logo.png on its dark ground, cut out and
coloured: the half above the water fog, the half below sea blue), the favicons, the Open Graph image (1200 x 630),
the screenshots (app/docs/img, the screen's own 240 x 240 pixels) and the play page: the browser emulator
(app/build/emu, made by ./build.sh --web) with the site's header, footer and colours around it, its own code
untouched; the River: fm1-base's protocol (base/web/backup) as it is, and the schema the firmware's code writes
(app/build/host's simulator, made by ./build.sh --host: "schema FILE"). Every page's links are checked at the end: a link to nothing stops the build.

Python 3.11+ with Pillow (.venv/bin/python). Usage: assemble.py [OUT_PARENT] [--no-emu] (default build/website;
only OUT_PARENT/frasil, river, perform and admin are replaced)."""
import functools
import hashlib
import html
import json
import math
import posixpath
import random
import re
import shutil
import subprocess
import sys
import tomllib
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urldefrag, urlsplit

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

REPO = Path(__file__).resolve().parents[1]
SRC = REPO / "website"
CONF = tomllib.loads((SRC / "site.toml").read_text(encoding="utf-8"))
SITE = CONF["base_path"].strip("/") or "frasil"   # the build's folder (build/website/SITE)
SERVE = "/" + (CONF["base_path"].strip("/") + "/" if CONF["base_path"].strip("/") else "")   # where it is served: / or /frasil/
ARGS = [a for a in sys.argv[1:] if not a.startswith("--")]
NO_EMU = "--no-emu" in sys.argv[1:]
OUT_PARENT = Path(ARGS[0]).resolve() if ARGS else REPO / "build" / "website"
OUT = OUT_PARENT / SITE
PERFORM = OUT_PARENT / "perform"          # Perform: its own site (perform.frasilsynth.com), the FM-1's play full screen
RIVER = OUT_PARENT / "river"              # the River: its own site (CONF["river_url"]), the main site's look
ADMIN = OUT_PARENT / "admin"              # the admin page (admin.frasilsynth.com): the River's accounts, the waitlists

FOG, WHITE, MIST, NIGHT = (220, 227, 234), (243, 246, 249), (139, 149, 164), (10, 13, 18)
SEA_HI, SEA, SEA_LO = (86, 160, 240), (52, 128, 236), (24, 70, 150)
FONTS = REPO / "base" / "assets" / "fonts"
BUTTONS = {"REC", "ARP", "SEQ", "SEL", "EDIT", "ENV", "LFO", "FX", "GLO", "HOME", "SAVE", "PLAY", "OCT−", "OCT+"}
ASSET_V = {}                              # assets/... -> a short hash of its content, for ?v= (caches)


def die(msg):
    raise SystemExit(f"website: {msg}")


# ================================================================ images
def premul_resize(im, size):
    """an RGBA image resized without dark fringes (through premultiplied alpha)"""
    return im.convert("RGBa").resize(size, Image.LANCZOS).convert("RGBA")


def lerp(a, b, t):
    return tuple(round(x + (y - x) * t) for x, y in zip(a, b))


# The logo on its ground (2752 x 1536): the letters span x 236..2517 and y 549..987. The upper half ends with the
# blade, a tapered line through the letters (rows 746..757); the lower half starts at row 779.
LOGO_BOX = (216, 531, 2540, 1003)
LOGO_BLADE = 751.5                        # the blade's middle row: the waterline
LOGO_SPLIT = 768                          # between the blade and the lower half


def logo_layers():
    """the two halves, cut out of the dark ground: (top, low), each the whole box, transparent elsewhere"""
    src = Image.open(REPO / "app" / "assets" / "frasil-logo.png").convert("RGB").crop(LOGO_BOX)
    w, h = src.size
    split = LOGO_SPLIT - LOGO_BOX[1]
    lum = src.convert("L")
    # alpha from luminance over the ground (about 14): the upper letters are fog (222), the lower ones a little greyer
    a_top = lum.point(lambda v: max(0, min(255, round((v - 24) * 255 / (220 - 24)))))
    a_low = lum.point(lambda v: max(0, min(255, round((v - 24) * 255 / (194 - 24)))))
    m_top = Image.new("L", (w, h), 0)
    m_top.paste(a_top.crop((0, 0, w, split)), (0, 0))
    m_low = Image.new("L", (w, h), 0)
    m_low.paste(a_low.crop((0, split, w, h)), (0, split))
    top = Image.new("RGBA", (w, h), FOG + (0,))
    top.putalpha(m_top)
    # below the water: sea blue, lighter just under the line, deeper and fainter further down
    col = Image.new("RGB", (1, h))
    fade = Image.new("L", (1, h))
    for y in range(h):
        t = max(0.0, (y - split) / (h - split))
        col.putpixel((0, y), lerp(SEA_HI, SEA, t / .45) if t < .45 else lerp(SEA, SEA_LO, (t - .45) / .55))
        fade.putpixel((0, y), round(255 * (1 - .38 * t)))
    low = col.resize((w, h)).convert("RGBA")
    low.putalpha(ImageChops.multiply(m_low, fade.resize((w, h))))
    return top, low


def make_images(img):
    top, low = logo_layers()
    w, h = top.size
    W = 2000
    H = round(h * W / w)
    premul_resize(top, (W, H)).save(img / "logo-top.png", optimize=True)
    premul_resize(low, (W, H)).save(img / "logo-low.png", optimize=True)
    duo = Image.alpha_composite(top, low)
    premul_resize(duo, (1000, round(h * 1000 / w))).save(img / "logo-duo.png", optimize=True)
    premul_resize(duo, (184, round(h * 184 / w))).save(img / "logo-mark.png", optimize=True)
    geo = {"logo_w": W, "logo_h": H, "logo_wl": f"{(LOGO_BLADE - LOGO_BOX[1]) / h:.4f}",
           "logo_split": f"{(LOGO_SPLIT - LOGO_BOX[1]) / h:.4f}"}
    make_favicons(img)
    make_og(img, top, low)
    make_strata(img)
    return geo


# The F, measured on the logo (its own pixels): stem x 364..455, top bar to x 650, middle bar to x 625 from y 733,
# the blade at 746..757, the water to 779, the letter from y 555 to 987.
def f_shapes(s, ox, oy):
    """the F as rectangles at scale s, origin (ox, oy) = the letter's top left: (above, below, blade)"""
    X = lambda x: ox + (x - 364) * s
    Y = lambda y: oy + (y - 555) * s
    above = [(X(364), Y(555), X(455), Y(746)), (X(364), Y(555), X(650), Y(634)), (X(364), Y(733), X(625), Y(746))]
    below = [(X(364), Y(779), X(455), Y(987)), (X(364), Y(779), X(625), Y(816))]
    blade = (Y(746), Y(757))
    return above, below, blade


def draw_f(size):
    """the favicon, drawn at 4x then reduced"""
    S = size * 4
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, S - 1, S - 1), radius=round(S * .22), fill=NIGHT + (255,))
    s = S * .70 / 432
    ox, oy = (S - 286 * s) / 2, S * .15
    above, below, (b0, b1) = f_shapes(s, ox, oy)
    for r in above:
        d.rectangle(r, fill=FOG + (255,))
    for r in below:
        d.rectangle(r, fill=SEA + (255,))
    # the blade: a tapered line across, the waterline
    mid, half = (b0 + b1) / 2, max(S * .018, (b1 - b0) / 2)
    d.polygon([(S * .08, mid), (S * .5, mid - half), (S * .92, mid), (S * .5, mid + half)], fill=WHITE + (255,))
    return im.reduce(4)


# The R (the River's), measured on the logo as the F is: stem x 713..803; above the blade, the bowl's outer curve from
# (921, 556) to (1045, 700) and down to (1038, 746), its counter x 803..955 from y 634; below, the bowl's foot to y 845
# (its right edge curving in from 1021 to 967) and the leg, a slant from x 875..971 at y 845 to 960..1056 at y 987.
def r_polys(s, ox, oy):
    """the R as polygons at scale s, origin (ox, oy) = its top left: (above, its counter, below)"""
    P = lambda pts: [(ox + (x - 713) * s, oy + (y - 555) * s) for x, y in pts]
    arc = lambda cx, cy, rx, ry, a0, a1, n=18: [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * k / n)),
                                                cy + ry * math.sin(math.radians(a0 + (a1 - a0) * k / n))) for k in range(n + 1)]
    outer = P([(713, 555), (921, 555), *arc(921, 700, 124, 145, -90, 0)[1:], (1043, 724), (1038, 746), (713, 746)])
    counter = P([(803, 634), (921, 634), *arc(921, 694, 34, 60, -90, 60)[1:], (803, 746)])
    below = [P([(713, 779), (803, 779), (803, 987), (713, 987)]),
             P([(713, 779), (1021, 779), (1013, 790), (1003, 802), (988, 814), (968, 826), (967, 845), (713, 845)]),
             P([(875, 845), (971, 845), (1056, 987), (960, 987)])]
    return outer, counter, below


# The P (Perform's): the R without its leg, its bowl closing on the same foot (x 713..1045)
def draw_r(size, leg=True):
    """the River's favicon: its R, as the F is drawn (4x, then reduced); leg False: Perform's P"""
    S = size * 4
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, S - 1, S - 1), radius=round(S * .22), fill=NIGHT + (255,))
    s = S * .70 / 432
    ox, oy = (S - (343 if leg else 332) * s) / 2, S * .15
    outer, counter, below = r_polys(s, ox, oy)
    below = below if leg else below[:2]
    d.polygon(outer, fill=FOG + (255,))
    d.polygon(counter, fill=NIGHT + (255,))
    for p in below:
        d.polygon(p, fill=SEA + (255,))
    _, _, (b0, b1) = f_shapes(s, ox, oy)
    mid, half = (b0 + b1) / 2, max(S * .018, (b1 - b0) / 2)
    d.polygon([(S * .08, mid), (S * .5, mid - half), (S * .92, mid), (S * .5, mid + half)], fill=WHITE + (255,))
    return im.reduce(4)


def river_favicon_svg(leg=True):
    """the same R as a vector: 64 x 64 (leg False: Perform's P)"""
    S = 64
    s = S * .70 / 432
    ox, oy = (S - (343 if leg else 332) * s) / 2, S * .15
    outer, counter, below = r_polys(s, ox, oy)
    below = below if leg else below[:2]
    path = lambda pts: "M" + "L".join(f"{x:.2f} {y:.2f}" for x, y in pts) + "Z"
    _, _, (b0, b1) = f_shapes(s, ox, oy)
    mid, half = (b0 + b1) / 2, max(S * .018, (b1 - b0) / 2)
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">'
            '<rect width="64" height="64" rx="14" fill="#0a0d12"/>'
            f'<path fill-rule="evenodd" d="{path(outer)}{path(counter)}" fill="#dce3ea"/>'
            + "".join(f'<path d="{path(p)}" fill="#3480ec"/>' for p in below)
            + f'<path d="M{S * .08:.2f} {mid:.2f}L32 {mid - half:.2f}L{S * .92:.2f} {mid:.2f}L32 {mid + half:.2f}Z" fill="#f3f6f9"/></svg>\n')


def favicon_svg():
    """the same F as a vector: 64 x 64"""
    S = 64
    s = S * .70 / 432
    above, below, (b0, b1) = f_shapes(s, (S - 286 * s) / 2, S * .15)
    rect = lambda r, c: f'<rect x="{r[0]:.2f}" y="{r[1]:.2f}" width="{r[2] - r[0]:.2f}" height="{r[3] - r[1]:.2f}" fill="{c}"/>'
    mid, half = (b0 + b1) / 2, max(S * .018, (b1 - b0) / 2)
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">'
            '<rect width="64" height="64" rx="14" fill="#0a0d12"/>'
            + "".join(rect(r, "#dce3ea") for r in above) + "".join(rect(r, "#3480ec") for r in below)
            + f'<path d="M{S * .08:.2f} {mid:.2f}L32 {mid - half:.2f}L{S * .92:.2f} {mid:.2f}L32 {mid + half:.2f}Z" fill="#f3f6f9"/></svg>\n')


def make_favicons(img):
    (img / "favicon.svg").write_text(favicon_svg(), encoding="utf-8")
    draw_f(32).save(img / "favicon-32.png", optimize=True)
    draw_f(180).save(img / "apple-touch-icon.png", optimize=True)
    draw_f(192).save(img / "icon-192.png", optimize=True)
    draw_f(512).save(img / "icon-512.png", optimize=True)
    draw_f(48).save(img / "favicon.ico", sizes=[(16, 16), (32, 32), (48, 48)])
    (img / "river-favicon.svg").write_text(river_favicon_svg(), encoding="utf-8")   # (the River's: its R)
    draw_r(32).save(img / "river-favicon-32.png", optimize=True)
    draw_r(180).save(img / "river-apple-touch-icon.png", optimize=True)
    draw_r(48).save(img / "river-favicon.ico", sizes=[(16, 16), (32, 32), (48, 48)])
    (img / "perform-favicon.svg").write_text(river_favicon_svg(leg=False), encoding="utf-8")   # (Perform's: its P)
    draw_r(32, leg=False).save(img / "perform-favicon-32.png", optimize=True)
    draw_r(180, leg=False).save(img / "perform-apple-touch-icon.png", optimize=True)
    draw_r(48, leg=False).save(img / "perform-favicon.ico", sizes=[(16, 16), (32, 32), (48, 48)])


def tracked(d, xy, text, font, fill, track, anchor="center"):
    """text drawn letter by letter with tracking (px); anchor center: xy is the line's middle"""
    widths = [d.textlength(c, font=font) for c in text]
    total = sum(widths) + track * (len(text) - 1)
    x = xy[0] - total / 2 if anchor == "center" else xy[0]
    for c, w in zip(text, widths):
        d.text((x, xy[1]), c, font=font, fill=fill, anchor="ls")
        x += w + track


def make_og(img, top, low):
    """the Open Graph image: the logo screen, wide"""
    W, H, WL = 1200, 630, 312
    rnd = random.Random(47)
    im = Image.new("RGB", (W, H))
    d = ImageDraw.Draw(im)
    for y in range(H):                    # the sky, then the water
        if y < WL:
            c = lerp((5, 7, 11), (12, 18, 27), (y / WL) ** 2.2)
        else:
            c = lerp((15, 27, 39), (7, 10, 15), min(1, (y - WL) / (H - WL)) ** .7)
        d.line([(0, y), (W, y)], fill=c)
    sky = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ds = ImageDraw.Draw(sky)
    for _ in range(190):                  # stars, sparser high up
        x, y = rnd.uniform(0, W), WL * (1 - rnd.random() ** .8)
        a = rnd.randint(60, 220)
        c = (110, 208, 255) if rnd.random() < .2 else FOG
        if rnd.random() < .06:
            r = rnd.uniform(3, 6)
            ds.line([(x - r, y), (x + r, y)], fill=c + (a,))
            ds.line([(x, y - r), (x, y + r)], fill=c + (a,))
        ds.point((x, y), fill=c + (a,))
    for _ in range(240):                  # frasil: needles and grains drifting above the line
        x, y = rnd.uniform(0, W), WL - abs(rnd.gauss(0, 38)) - 6
        a = rnd.randint(40, 160)
        if rnd.random() < .45:
            ds.line([(x, y), (x + rnd.uniform(2, 5), y)], fill=FOG + (a,))
        else:
            ds.point((x, y), fill=FOG + (a,))
    im.paste(sky, (0, 0), sky)
    # the water's strokes of light
    wat = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    dw = ImageDraw.Draw(wat)
    for _ in range(150):
        y = WL + 8 + (H - WL - 8) * rnd.random() ** 1.6
        x, ln = rnd.uniform(0, W), rnd.uniform(4, 30) * (1 - (y - WL) / (H - WL) * .4)
        dw.line([(x, y), (x + ln, y)], fill=(160, 180, 200, rnd.randint(18, 70)))
    im.paste(wat, (0, 0), wat)
    # the logo: the blade on the waterline; the lower half swaying a little
    lw = 840
    lh = round(top.height * lw / top.width)
    t = premul_resize(top, (lw, lh))
    lo = premul_resize(low, (lw, lh))
    lx = (W - lw) // 2
    ly = round(WL - (LOGO_BLADE - LOGO_BOX[1]) / top.height * lh)
    sway = Image.new("RGBA", (lw + 20, lh), (0, 0, 0, 0))
    split = (LOGO_SPLIT - LOGO_BOX[1]) / top.height
    for y in range(lh):
        depth = max(0.0, (y / lh - split) / (1 - split))
        dx = 10 + (.4 + 2.2 * depth) * math.sin(y * .085 + .6) + (.2 + 1.1 * depth) * math.sin(y * .031 + 1.9)
        row = lo.crop((0, y, lw, y + 1))
        fx = dx - math.floor(dx)                   # a fraction of a pixel: two rows blended, no stair steps
        sway.alpha_composite(Image.blend(row, Image.new("RGBA", row.size), fx), (math.floor(dx), y))
        sway.alpha_composite(Image.blend(Image.new("RGBA", row.size), row, fx), (math.floor(dx) + 1, y))
    pad = 60                                       # a soft glow behind the letters above the water
    halo = Image.new("L", (lw + 2 * pad, lh + 2 * pad), 0)
    halo.paste(t.getchannel("A"), (pad, pad))
    halo = halo.filter(ImageFilter.GaussianBlur(18)).point(lambda v: v * 40 // 255)
    im.paste((150, 190, 230), (lx - pad, ly - pad), halo)
    im.paste(sway, (lx - 10, ly), sway)
    im.paste(t, (lx, ly), t)
    d = ImageDraw.Draw(im)
    for x in range(W):                    # the waterline: slate at the sides, fog in the middle
        k = 1 - min(1, abs(x - W / 2) / (W / 2)) ** 1.5
        d.point((x, WL), fill=lerp((22, 28, 36), FOG, k * .8))
    f1 = ImageFont.truetype(str(FONTS / "BarlowSemiCondensed-SemiBold.ttf"), 25)
    f2 = ImageFont.truetype(str(FONTS / "BarlowSemiCondensed-Medium.ttf"), 22)
    tracked(d, (W / 2, 470), "DRONE TEXTURES FROZEN IN MOTION", f1, (160, 172, 188), 8)
    tracked(d, (W / 2, 520), "FOR THE M-VAVE FM-1  ·  BY STYVES GOUPIL", f2, (110, 122, 138), 4)
    im.save(img / "og.png", optimize=True)


def make_strata(img):
    """the play page's three layers' strata, cut from the real screen: the drone's mix as the device draws it (ui.c's
    strata(): a stratum every 14 rows from 126, under the waterline at 116; WARMTH's, the fourth, at 168)"""
    sh = Image.open(REPO / "app" / "docs" / "img" / "screen-play.png").convert("RGB")
    for name, y in (("choir", 126), ("partials", 140), ("breath", 154)):
        sh.crop((78, y - 5, 206, y + 5)).save(img / f"stratum-{name}.png", optimize=True)


def copy_screens(dst):
    dst.mkdir(parents=True, exist_ok=True)
    n = 0
    for f in sorted((REPO / "app" / "docs" / "img").glob("screen-*.png")):
        Image.open(f).save(dst / f.name, optimize=True)
        n += 1
    return n


# ================================================================ the screenshots, the harmony's widget
# name: (title, caption, alt). Titles and alts from the manual where it has them (app/docs/MANUAL.md).
SHOTS = {
    "play": ("The play page", "Three voices sounding, the drone's mix under the waterline.",
             "The play page: the scene, NEW SHARD, with an amber dot (the sound changed) and the keys' badge (DRONE, octave "
             "-1) in the header; three voices as currents of ice in the river; the drone's mix and WARMTH as four strata "
             "under the waterline; KEYS (DRONE), the drone's preset (CUSTOM) and BODY at the bottom"),
    "pressure": ("Pressure", "A held key pressed in: its voice glows amber.",
                 "The play page with a held key pressed in: its voice glows amber, its note names in lamp light"),
    "glints": ("GLINT", "SELECT on GLINT, then a few keys: notes shining above the river.",
               "GLINT notes shining above the river, ARP LATCH at its top right; the keys' badge blue: ARP, octave -1"),
    "freeze": ("FREEZE", "FX held: the crystals set into a lattice.",
               "FREEZE on: the FREEZE tag in the header, the crystals set into a lattice, the water still, the bass's D2 "
               "on the right"),
    "bass": ("BASS", "SELECT on BASS, then a key: a slow swell across the river.",
             "The bass under three drones: a slow swell across the river, D2 on the right, the keys' badge orange"),
    "overlay": ("A left encoder turned", "PRESETS: the drone's preset, large over the river.",
                "PRESETS turned: the drone's preset, EMBER, large over the river, 06/24 at the bottom"),
    "mixer": ("MIXER", "HOME on the play page: four faders and their meters.",
              "MIXER: the four faders, DRONE, BASS, GLINT and CLOUD at 100, and their meters"),
    "genfx": ("GENERATIVE FX", "FX: SWELL, SPARKLE, BLUR, SINK.",
              "GENERATIVE FX, FX's first page: SWELL 0, SPARKLE 31, BLUR 24, SINK 0"),
    "cloud": ("CLOUD", "FX twice: SIZE, DECAY, TONE and FREEZE.",
              "CLOUD, FX's second page: SIZE 75, DECAY 9.0 s, TONE 50, FREEZE OFF"),
    "harmony": ("HARMONY", "SEL: D, MINOR, TRIAD, CLOSE.", "HARMONY: D, MINOR, TRIAD, CLOSE"),
    "glint-params": ("GLINT I", "ARP: PRESET, TYPE, LEVEL, BRIGHT.",
                     "GLINT I, ARP's first page: PRESET SINE, TYPE ARP, LEVEL 71, BRIGHT 0; the keys' badge: ARP"),
    "glint-arp": ("GLINT ARP", "ARP five times, TYPE on ARP.",
                  "GLINT ARP, ARP's fifth page with TYPE on ARP: MODE UPDOWN, RATE 1/8, RANGE 2 octaves, CHANCE 100"),
    "glint-seq": ("GLINT SEQ", "REC, or ARP five times with TYPE on SEQ: sixteen steps, an accent on each beat.",
                  "GLINT SEQ, ARP's fifth page with TYPE on SEQ, the keys' badge GLINT (played as they are, to practise): STEP 1, NOTE D4, ACCENT ON, RECORD OFF; "
                  "above, the sequence as a "
                  "constellation of sixteen stars, each as high as its note, the accents larger, the step playing "
                  "with a line of light through it"),
    "bass-page": ("BASS", "SEQ: MODEL, DRIVE, FILTER, RESONANCE.",
                  "BASS, SEQ's first page: MODEL SINE, DRIVE 16, FILTER 837 Hz, RESONANCE 16"),
    "bass-adsr": ("BASS ADSR", "SEQ three times: the envelope, a held note riding it.",
                  "BASS ADSR, SEQ's third page: ATTACK 26 ms, DECAY 210 ms, SUSTAIN 41, RELEASE 347 ms; above, the "
                  "envelope drawn in orange, each part as wide as its knob is turned, and the held note's glint waiting at "
                  "the end of the SUSTAIN plateau"),
    "glint-adsr": ("GLINT ADSR", "ARP three times: the envelope, glints riding it.",
                   "GLINT ADSR, ARP's third page with WARM LEAD: ATTACK 4 ms, DECAY 2.0 s, SUSTAIN 83, RELEASE 995 ms; "
                   "above, the envelope drawn in ice blue, a glint just struck at its top and two let go on its RELEASE"),
    "lfo": ("LFO 1", "LFO twice: a slow sine, and its glint.",
            "LFO 1, LFO's second page: SHAPE SINE, its RATE and DEPTH, POLARITY BIPOLAR; the sine across the strip "
            "above, over one turn, a glint where the LFO is now"),
    "lfo-targets": ("LFO 1 TARGETS", "LFO three times: FILTER, TIMBRE, GL LEVEL, BS FILTER.",
                    "LFO 1 TARGETS, LFO's third page: TARGET 1 FILTER and TARGET 2 TIMBRE in white, TARGET 3 GL LEVEL "
                    "in ice blue, TARGET 4 BS FILTER in orange; the LFO's sine above"),
    "scenes": ("SCENES", "SAVE: NEW SHARD loaded, slot 02 picked; MORPH, LOAD.",
               "SCENES: NEW SHARD loaded (NOW), slot 02, LUMIERE NOCTURNE, picked among the factory scenes; in a box of "
               "their own, PLAY LOAD and SAVE + REC STORE; MORPH CUT, LOAD SND+NOTES"),
    "logo": ("The logo screen", "HOME on the MIXER: the logo, alive.",
             "FRASIL's logo screen: the name half under the waterline, granules of frasil drifting in the sky, by Styves Goupil"),
    "global": ("GLOBAL", "GLO: TEMPO, LOW CUT, TUNE, KEY MODE; the load in the header.",
               "GLOBAL, GLO's first page: TEMPO 90 BPM, LOW CUT 38 Hz, TUNE 0 ct, KEY MODE LATCH; the processor's load in the header"),
    "help-1": ("Help", "Hold HOME: each button's job first, then the button.",
               "Help: each button's job first, then the button: GENERATIVE FX on FX (again: CLOUD, PUMP; hold: FREEZE), "
               "HARMONY on SEL (hold and a key: a single note), DRONE SHAPE on ENV (hold: SWELL), DRONE MOTION on LFO (again: "
               "LFO 1 to 3; hold and a K: route)"),
    "help-3": ("Help, the buttons", "Turn SELECT for the next page.",
               "Help: GLINT PAGES on ARP (hold: ARP LATCH), BASS PAGES on SEQ, START, STOP on PLAY (hold: "
               "release all; on scenes: load), RECORD LIVE on REC (again: off; hold: the metronome)"),
    "help-4": ("Help, the encoders", "KEYS, the drone's preset, BODY and the octave.",
               "Help: KEYS on SELECT (DRONE, GLINT, BASS), PRESET on PRESETS and BODY on ALGORITHM, each the "
               "drone's; OCTAVE on OCT- and OCT+, both together to reset"),
    "help-5": ("Help, the keys", "Light a drone, let it go, swell it.",
               "Help: light a drone (tap; a chord with HARMONY), let it go (tap again), swell it (press in: hold the "
               "key, press harder), one note over a chord (SEL and a key)"),
}
GALLERY = ["play", "pressure", "glints", "freeze", "bass", "overlay", "mixer", "genfx", "harmony", "glint-seq", "scenes", "logo"]
GLOW = {"pressure": " glow-lamp", "bass": " glow-lamp", "freeze": " glow-ice", "glints": " glow-ice", "glint-arp": " glow-ice",
        "glint-seq": " glow-ice", "glint-params": " glow-ice",
        "lfo": " glow-ice", "lfo-targets": " glow-ice"}


def shot_html(root, name, cls=""):
    title, cap, alt = SHOTS[name]
    src = f"{root}assets/screens/screen-{name}.png"
    e = html.escape
    return (f'<figure class="shot{cls}"><div class="screen{GLOW.get(name, "")}">'
            f'<img src="{src}" width="240" height="240" alt="{e(alt)}" loading="lazy" decoding="async">'
            f'<button class="zoom" type="button" data-zoom="{src}" data-alt="{e(alt)}" data-cap="{e(title)}: {e(cap)}">'
            f'<span class="sr-only">Enlarge the screen: {e(title)}</span></button></div>'
            f'<figcaption><span class="label">{e(title)}</span><span data-nocaps>{e(cap)}</span></figcaption></figure>')


def shots(root):
    return {"shot_" + n.replace("-", "_"): shot_html(root, n) for n in SHOTS}


def gallery(root):
    return "\n".join(f'<li class="rv" style="--d:{(i % 4) * .06:.2f}s">{shot_html(root, n)}</li>' for i, n in enumerate(GALLERY))


NOTES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
SCALES = ["CHROMATIC", "MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN", "LOCRIAN", "HARMONIC", "MELODIC",
          "PENTA MAJ", "PENTA MIN"]
CHORDS = ["SINGLE", "FIFTH", "TRIAD", "SEVENTH", "SUS2", "SUS4", "ADD9", "QUARTAL", "MAJ", "MIN", "MAJ7", "MIN7", "DOM7",
          "AUG", "DIM"]


def harmony_widget():
    """HARMONY's four settings and the FM-1's 27 keys; js/harmony.js plays them (app/harmony.c, in the browser)"""
    def sel(n, key, name, opts, cur, groups=None):
        o = []
        for i, v in enumerate(opts):
            if groups and i in groups:
                if i:
                    o.append("</optgroup>")
                o.append(f'<optgroup label="{groups[i]}">')
            o.append(f'<option value="{i}"{" selected" if i == cur else ""}>{v}</option>')
        if groups:
            o.append("</optgroup>")
        return (f'<label class="h-sel"><span class="h-lab"><span class="disc">{n}</span>{name}</span>'
                f'<select data-h="{key}">{"".join(o)}</select></label>')
    return ('<div class="harmony rv" data-harmony data-nocaps>'
            '<div class="h-ctl">'
            + sel(1, "root", "ROOT", NOTES, 2) + sel(2, "scale", "SCALE", SCALES, 2)
            + sel(3, "chord", "CHORD", CHORDS, 2, {0: "In the scale", 8: "Fixed"})
            + sel(4, "voicing", "VOICING", ["CLOSE", "OPEN", "WIDE"], 0)
            + '</div><p class="h-swipe" aria-hidden="true">Swipe for all 27 keys</p><div class="h-board"><div class="h-keys" role="group" aria-label="The FM-1\'s 27 keys, F2 to G4 at '
              'octave −1: tap one to hear what it plays, in note names"></div></div>'
            '<p class="h-out" aria-live="polite"><span class="label">D3</span> plays <b>D3 · F3 · A3</b>: in D MINOR '
            'with TRIAD, a chord of the scale’s own notes.</p></div>')


# ================================================================ pages
def changelog_html():
    """the repo's CHANGELOG.md as the site's changelog page: each version a section (#v2-5), its paragraphs, its lists,
    its bold leads. Only the Markdown it uses: ## a version, **a heading** alone on its line, - a list (continued by
    lines indented two spaces), **bold**, `code`, [links](...)"""
    def inline(s):
        s = html.escape(s, quote=False)
        s = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", lambda m: f'<a class="link" href="{m[2]}">{m[1]}</a>', s)
        s = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", s)
        return re.sub(r"`([^`]+)`", r"<code>\1</code>", s)
    out, vers, para, item, in_list, in_ver = [], [], [], [], False, False
    def flush_para():
        if para:
            out.append(f"<p>{inline(' '.join(para))}</p>")
            para.clear()
    def flush_item():
        if item:
            out.append(f"<li>{inline(' '.join(item))}</li>")
            item.clear()
    def close_list():
        nonlocal in_list
        flush_item()
        if in_list:
            out.append("</ul>")
            in_list = False
    for line in (REPO / "CHANGELOG.md").read_text(encoding="utf-8").splitlines():
        s = line.strip()
        if line.startswith("# "):
            continue
        m = re.match(r"## (\S+)(?: \((.*)\))?\s*$", line)
        if m:
            flush_para(); close_list()
            if in_ver:
                out.append("</section>")
            vid = "v" + m[1].replace(".", "-")
            vers.append((m[1], vid))
            when = f' <span class="cl-when">{html.escape(m[2])}</span>' if m[2] else ""
            out.append(f'<section class="cl-ver" id="{vid}" aria-labelledby="{vid}-h"><h2 id="{vid}-h">{html.escape(m[1])}{when}</h2>')
            in_ver = True
        elif re.fullmatch(r"\*\*[^*]+\*\*", s):
            flush_para(); close_list()
            out.append(f'<h3 class="label">{html.escape(s[2:-2])}</h3>')
        elif line.startswith("- "):
            flush_para(); flush_item()
            if not in_list:
                out.append('<ul role="list">')
                in_list = True
            item.append(line[2:].strip())
        elif line.startswith("  ") and item:
            item.append(s)
        elif not s:
            flush_para(); flush_item()
        else:
            close_list()
            para.append(s)
    flush_para(); close_list()
    if in_ver:
        out.append("</section>")
    jump = " ".join(f'<a href="#{vid}">{html.escape(v)}</a>' for v, vid in vers)
    return jump, "\n".join(out)


def read_page(path):
    """a page's metadata (its first comment: key: value lines) and its HTML"""
    text = path.read_text(encoding="utf-8")
    m = re.match(r"\s*<!--(.*?)-->\s*", text, re.S)
    meta = {}
    if m:
        for line in m.group(1).strip().splitlines():
            if ":" in line:
                k, v = line.split(":", 1)
                meta[k.strip()] = v.strip()
        text = text[m.end():]
    return meta, text


def fill(tpl, values, what):
    for k, v in values.items():
        tpl = tpl.replace("{{" + k + "}}", str(v))
    left = sorted(set(re.findall(r"\{\{[a-z_]+\}\}", tpl)))
    if left:
        die(f"{what}: {' '.join(left)} not filled in")
    return tpl


def asset(root, path):
    return f"{root}assets/{path}?v={ASSET_V[path]}" if path in ASSET_V else f"{root}assets/{path}"


VOID = {"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "source", "track", "wbr"}
SKIP = {"code", "pre", "script", "style", "svg", "kbd", "option", "textarea", "title", "button", "select"}
CAPS = re.compile(
    r"(?<![\w&#;/.\-−])"
    r"((?:OCT[−+])|(?:K[1-4](?:–K[1-4])?)|"
    r"(?:(?:\d+\s(?=[A-Z]{2}))?[A-Z][A-Z0-9]*(?:-[A-Z][A-Z0-9]+)*(?:\s(?=[A-Z0-9])(?:[A-Z][A-Z0-9]*(?:-[A-Z][A-Z0-9]+)*|\d+))*))"
    r"(?=s\b|[^\w\-]|$)"
)


NOT_THE_INSTRUMENT = {"README", "SDK", "AC79", "V15", "USB", "TRS"}
BADGE = re.compile(r"K[1-4](?:–K[1-4])?")


def caps_word(m):
    s = m.group(1)
    if s.startswith("M-VAVE"):
        return s
    words = s.split(" ")
    if (words[0] == "LFO" and len(words) > 1 and words[1] in ("1", "2")) or \
            (words[0] in ("GLINT", "GENERATIVE") and any(w in BUTTONS for w in words[1:])) or \
            (s in BUTTONS and m.string[m.end():m.end() + 1] == "s") or \
            (s == "LFO" and re.search(r"\b(an|the|each|one|its|that|this)\s$", m.string[max(0, m.start() - 6):m.start()], re.I)):
        return f'<span class="k">{s}</span>'      # LFO 1, GLINT SEQ II, GENERATIVE FX, the LFOs: the things, not the button
    if len(words) > 1 and any(w in BUTTONS or BADGE.fullmatch(w) or w in NOT_THE_INSTRUMENT for w in words):
        out, run = [], []                     # a button or a badge inside a phrase: each on its own
        for w in words + [None]:
            if w is None or w in BUTTONS or BADGE.fullmatch(w) or w in NOT_THE_INSTRUMENT:
                if run:
                    out.append(caps_word(re.match(r"(.*)", " ".join(run))))
                    run = []
                if w is not None:
                    out.append(caps_word(re.match(r"(.*)", w)))
            else:
                run.append(w)
        return " ".join(out)
    if s in NOT_THE_INSTRUMENT:
        return s
    if s in BUTTONS:
        return f'<span class="key">{s}</span>'
    if BADGE.fullmatch(s):
        return f'<span class="kn">{s}</span>'
    if not re.search(r"[A-Z][A-Z0-9]", s):
        return s
    return f'<span class="k">{s}</span>'


def quiet_discs(src):
    """a knob's badge is a picture of the panel: its letter or number stays out of what a screen reader says"""
    return re.sub(r'<span class="disc(?=[ "])', '<span aria-hidden="true" class="disc', src)


def capsify(src):
    """the instrument's words in running text, in its own voice: inside [data-caps], CHOIR becomes
    <span class="k">CHOIR</span>, a button a framed key, K1–K4 a badge; not in code, headings' anchors, or [data-nocaps]"""
    out, stack, on, off = [], [], 0, 0
    for part in re.split(r"(<[^>]+>)", src):
        if part.startswith("<"):
            out.append(part)
            m = re.match(r"<\s*(/)?\s*([a-zA-Z0-9]+)", part)
            if not m or part.startswith("<!"):
                continue
            close, tag = m.group(1), m.group(2).lower()
            if close:
                while stack:
                    t, c, n = stack.pop()
                    on -= c
                    off -= n
                    if t == tag:
                        break
            elif tag not in VOID and not part.endswith("/>"):
                c = 1 if "data-caps" in part else 0
                n = 1 if (tag in SKIP or "data-nocaps" in part or 'class="k' in part or 'class="key' in part
                          or 'class="disc' in part or 'class="label' in part) else 0
                stack.append((tag, c, n))
                on += c
                off += n
        elif on > 0 and off == 0 and part.strip():
            out.append(CAPS.sub(caps_word, part))
        else:
            out.append(part)
    return "".join(out)


NOBREAK = re.compile(r"M-VAVE FM-1|M-VAVE|FM-1(?!_)")


def nobreak(src):
    """M-VAVE and FM-1 never split at their hyphen (outside tags, code and scripts)"""
    out, skip = [], 0
    for part in re.split(r"(<[^>]+>)", src):
        if part.startswith("<"):
            m = re.match(r"<\s*(/)?\s*(code|pre|script|style|title|textarea)\b", part, re.I)
            if m:
                skip += -1 if m.group(1) else 1
            out.append(part)
        elif skip <= 0 and "-" in part:
            out.append(NOBREAK.sub(lambda m: f'<span class="nb">{m.group(0)}</span>', part))
        else:
            out.append(part)
    return "".join(out)


def slug(s):
    s = re.sub(r"<[^>]+>", "", s)
    s = html.unescape(s).lower().replace("&", "and").replace("'", "").replace("’", "")
    return re.sub(r"[^a-z0-9]+", "-", s).strip("-")


def anchor_headings(src):
    """h2 and h3 get an id (from their text if they have none) and a link to themselves; returns (html, [(id, text)])"""
    toc, seen = [], set()

    def one(m):
        tag, attrs, inner = m.group(1), m.group(2), m.group(3)
        idm = re.search(r'\sid="([^"]+)"', attrs)
        hid = idm.group(1) if idm else slug(inner)
        while hid in seen:
            hid += "-2"
        seen.add(hid)
        if not idm:
            attrs += f' id="{hid}"'
        if tag == "h2":
            toc.append((hid, re.sub(r"<[^>]+>", "", inner).strip()))
        link = f'<a class="h-anchor" href="#{hid}"><span class="sr-only">Link to this section</span></a>'
        return f"<{tag}{attrs}>{inner}{link}</{tag}>"

    return re.sub(r"<(h[23])([^>]*)>(.*?)</\1>", one, src, flags=re.S), toc


def label_tables(src):
    """a table.params's cells carry their column's heading (data-label), for the phone's stacked layout"""
    def table(m):
        t = m.group(0)
        if "</thead>" not in t:
            return t
        thead, body = t.split("</thead>", 1)
        heads = [html.unescape(re.sub(r"<[^>]+>", "", h)).strip() for h in re.findall(r"<th[^>]*>(.*?)</th>", thead, re.S)]

        def row(rm):
            col = 0

            def cell(cm):
                nonlocal col
                attrs = cm.group(2) or ""
                span = re.search(r'colspan="(\d+)"', attrs)
                lab = heads[col] if col < len(heads) else ""
                col += int(span.group(1)) if span else 1
                if "data-label" in attrs:
                    return cm.group(0)
                return f'<{cm.group(1)}{attrs} data-label="{html.escape(lab)}">'
            return re.sub(r"<(td|th)(\s[^>]*)?>", cell, rm.group(0))
        return thead + "</thead>" + re.sub(r"<tr[^>]*>.*?</tr>", row, body, flags=re.S)
    return re.sub(r'<table class="params[^"]*">.*?</table>', table, src, flags=re.S)


def keys_html(spec):
    """'REC > ARP' -> the framed buttons, one after the other"""
    if not spec:
        return ""
    parts = []
    for i, k in enumerate(s.strip() for s in spec.split(">")):
        if i:
            parts.append('<span class="then" aria-hidden="true">›</span>')
        parts.append(f'<span class="key">{html.escape(k)}</span>')
    return "".join(parts)


PRELOAD_FONTS = ("barlow-semi-condensed-normal-500", "barlow-semi-condensed-normal-600", "barlow-semi-condensed-normal-700",
                 "newsreader-normal-300-600", "newsreader-italic-300-500")   # (assets/fonts/, their latin)


# Discord's mark (Simple Icons, CC0), drawn in the text's colour
DISCORD_SVG = ('<svg class="dc-mark" viewBox="0 0 24 24" aria-hidden="true" focusable="false"><path d="M20.317 4.3698a19.7913 '
               '19.7913 0 00-4.8851-1.5152.0741.0741 0 00-.0785.0371c-.211.3753-.4447.8648-.6083 1.2495-1.8447-.2762-3.68-'
               '.2762-5.4868 0-.1636-.3933-.4058-.8742-.6177-1.2495a.077.077 0 00-.0785-.037 19.7363 19.7363 0 00-4.8852 '
               '1.515.0699.0699 0 00-.0321.0277C.5334 9.0458-.319 13.5799.0992 18.0578a.0824.0824 0 00.0312.0561c2.0528 '
               '1.5076 4.0413 2.4228 5.9929 3.0294a.0777.0777 0 00.0842-.0276c.4616-.6304.8731-1.2952 1.226-1.9942a.076.076 '
               '0 00-.0416-.1057c-.6528-.2476-1.2743-.5495-1.8722-.8923a.077.077 0 01-.0076-.1277c.1258-.0943.2517-.1923.3718'
               '-.2914a.0743.0743 0 01.0776-.0105c3.9278 1.7933 8.18 1.7933 12.0614 0a.0739.0739 0 01.0785.0095c.1202.099.246'
               '.1981.3728.2924a.077.077 0 01-.0066.1276 12.2986 12.2986 0 01-1.873.8914.0766.0766 0 00-.0407.1067c.3604.698'
               '.7719 1.3628 1.225 1.9932a.076.076 0 00.0842.0286c1.961-.6067 3.9495-1.5219 6.0023-3.0294a.077.077 0 00.0313-'
               '.0552c.5004-5.177-.8382-9.6739-3.5485-13.6604a.061.061 0 00-.0312-.0286zM8.02 15.3312c-1.1825 0-2.1569-1.0857-'
               '2.1569-2.419 0-1.3332.9555-2.4189 2.157-2.4189 1.2108 0 2.1757 1.0952 2.1568 2.419 0 1.3332-.9555 2.4189-'
               '2.1569 2.4189zm7.9748 0c-1.1825 0-2.1569-1.0857-2.1569-2.419 0-1.3332.9554-2.4189 2.1569-2.4189 1.2108 0 '
               '2.1757 1.0952 2.1568 2.419 0 1.3332-.946 2.4189-2.1568 2.4189Z"/></svg>')


# The ways to FRASIL, the same in the landing's fold and in the menu (every page's, the River's too): (title, what,
# where on the site or a site.toml key for another site, None while it is to come, its icon)
WAYS = (("Play in browser", "Nothing to install", "play/", "browser"),
        ("Play on FM-1", "Install the firmware", "guide/install/", "fm1"),
        ("Play on iPad", "Coming soon", None, "ipad"),
        ("FRASIL VST", "Coming soon", None, "vst"))
WAITLISTS = {"ipad": "FRASIL for iPad", "vst": "FRASIL VST"}   # (what is to come: its dialog's title; server/waitlist.py LISTS)
APPS = (("FRASIL River", "Scenes management & sharing", "river_url", "river-favicon.svg"),
        ("FRASIL Perform", "Second screen for your FM-1", "perform_url", "perform-favicon.svg"))
WAY_ICONS = {   # 24 x 24, drawn in the current colour
    "browser": '<rect x="2.5" y="4.5" width="19" height="15" rx="2.5"/><path d="M2.5 8.5h19"/><path d="M10.2 11.4v5.2l4.3-2.6z" fill="currentColor" stroke="none"/>',
    "fm1": '<rect x="2" y="5.5" width="20" height="13" rx="2.2"/><circle cx="6.2" cy="9.4" r="1.1"/><circle cx="10" cy="9.4" r="1.1"/><path d="M2 13h20M7 13v5.5M12 13v5.5M17 13v5.5"/>',
    "ipad": '<rect x="2.5" y="5" width="19" height="14" rx="2.6"/><path d="M19.2 12h.01" stroke-width="2.2" stroke-linecap="round"/>',
    "vst": '<rect x="2.5" y="5" width="19" height="14" rx="2"/><circle cx="8.5" cy="12.3" r="2.6"/><circle cx="15.5" cy="12.3" r="2.6"/><path d="M8.5 12.3V9.7M15.5 12.3l1.8-1.8"/>',
}


def ways_html(site, img, cls="way"):
    """the ways, as links (or what is to come, dashed: a button to its waitlist), then the apps: site the main site's root (a relative root, or its
    URL from the River), img where the favicons are"""
    out = []
    for title, what, href, ico in WAYS:
        svg = (f'<svg class="way-ico" viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" '
               f'stroke-width="1.5" stroke-linejoin="round">{WAY_ICONS[ico]}</svg>')
        inner = f'{svg}<span><b>{title}</b><small>{what}</small></span>'
        out.append(f'<li><a class="{cls}" href="{site}{href}">{inner}</a></li>' if href else   # (to come: its waitlist,
                   f'<li><button class="{cls} soon" type="button" data-waitlist="{ico}" '          # js/site.js's dialog)
                   f'data-title="{html.escape(WAITLISTS[ico])}" aria-haspopup="dialog">{inner}</button></li>')
    apps = [f'<li><a class="{cls} app" href="{html.escape(CONF.get(key, ""))}"><img class="way-ico" src="{img}{icon}" width="24" '
            f'height="24" alt=""><span><b>{title}</b><small>{what}</small></span></a></li>' for title, what, key, icon in APPS]
    return "".join(out), "".join(apps)


def menu_html(site, img, cur=""):
    """the menu (its button and its panel), every page's: the site's three, the ways, the apps, Discord, the status"""
    pages = (("Overview", "", "home"), ("Guide", "guide/", "guide"), ("Cheat sheet", "cheatsheet/", "cheat"))   # (installing:
                                                                                                    # Play on FM-1)
    ways, apps = ways_html(site, img, "mway")
    lit = ' aria-current="true"'
    main = "".join(f'<li><a href="{site}{h}"{lit if k == cur else ""}>{n}</a></li>' for n, h, k in pages)
    return ('<button class="menu-btn" type="button" aria-expanded="false" aria-controls="site-menu"><span class="sr-only">Menu'
            '</span><i></i><i></i><i></i></button>\n'
            f'<nav class="site-menu" id="site-menu" aria-label="Everything FRASIL" hidden><ul class="menu-main" role="list">{main}</ul>'
            f'<ul class="menu-ways" role="list">{ways}</ul><ul class="menu-ways" role="list">{apps}</ul>'
            f'<div class="menu-foot">{discord_links()["discord_menu"]}<span class="status">{html.escape(CONF["status"])}</span></div></nav>')


def discord_links():
    """the Discord server (site.toml discord): in the main site's header and the River's (its mark, the word when there
    is room), in the footer's Elsewhere; in a tab of its own. Nothing while there is no invite"""
    url = html.escape(CONF.get("discord", ""))
    if not url:
        return {"discord_nav": "", "discord_foot": "", "discord_menu": "", "discord_head": ""}
    a = f'<a class="nav-dc" href="{url}" target="_blank" rel="noopener" aria-label="Discord">'
    return {"discord_nav": f'    {a}{DISCORD_SVG}<span class="nav-long">Discord</span></a>\n',
            "discord_foot": f'          <li><a href="{url}" target="_blank" rel="noopener">Discord</a></li>\n',
            "discord_menu": f'<a class="nav-dc" href="{url}" target="_blank" rel="noopener">{DISCORD_SVG}<span>Discord</span></a>',
            "discord_head": f'<a class="head-dc" href="{url}" target="_blank" rel="noopener" aria-label="Discord">{DISCORD_SVG}<span>Discord</span></a>'}


class Site:
    def __init__(self):
        t = SRC / "templates"
        self.base, self.head, self.header, self.footer, self.guide_tpl = (
            (t / f).read_text(encoding="utf-8") for f in ("base.html", "head.html", "header.html", "footer.html", "guide.html"))

    def common(self, root, section):
        cur = lambda s: ' aria-current="true"' if s == section else ""
        return {"root": root, "repo": CONF["repo"], "releases": CONF["releases"], "hub": CONF["hub"],
                "river": CONF["river_url"], "perform": CONF.get("perform_url", "https://perform.frasilsynth.com/"),
                "changelog_link": f'          <li><a href="{root}changelog/">Changelog</a></li>\n', "status": html.escape(CONF["status"]), "beta_note": html.escape(CONF["beta_note"]),
                # the header's five, the same on every page (only which one is lit changes): the guide's Install page
                # lights Install, the River library page River library
                "cur_home": cur("home"), "cur_guide": cur("guide"), "cur_river": cur("river"), "cur_play": cur("play"),
                "cur_cheat": cur("cheat"), "menu": menu_html(root, root + "assets/img/", section),
                "ways": "".join(ways_html(root, root + "assets/img/")),
                "cur_install": cur("install"), "year": "2026",
                # the footer's River library, cheat sheet and credits: the main site's, on FRASIL River's pages too
                "river_lib": f'          <li><a href="{root}river/">River library</a></li>\n',
                "cheat_link": f'          <li><a href="{root}cheatsheet/">Cheat sheet</a></li>\n',
                "cheat_pdf": asset(root, "cheatsheet/frasil-cheatsheet.pdf"),
                "cheat_png": asset(root, "cheatsheet/frasil-cheatsheet.png"),
                "credits_link": f'          <li><a href="{root}credits/">Credits</a></li>\n',
                **discord_links(),
                "harmony": harmony_widget(), **shots(root)}

    def head_html(self, root, rel, title, description, og_title=None, canonical=None, assets=None, icon=None):
        return self.head_fill(root, rel, title, description, og_title, canonical, assets, icon) + engine_meta()

    def head_fill(self, root, rel, title, description, og_title=None, canonical=None, assets=None, icon=None):
        # the fonts every page sets its header and titles in, asked for at once (base.css declares them all)
        a = root if assets is None else assets
        preload = "\n".join(f'<link rel="preload" href="{a}assets/fonts/{f}-latin.woff2" as="font" type="font/woff2" crossorigin>'
                             for f in PRELOAD_FONTS)
        icons = {"icon_svg": f"{a}assets/img/{icon or ''}favicon.svg", "icon_png": f"{a}assets/img/{icon or ''}favicon-32.png",
                 "icon_apple": f"{a}assets/img/{icon or ''}apple-touch-icon.png"}   # (icon: "river-", the River's own)
        return fill(self.head, {**self.common(root, ""), **icons, "title": html.escape(title), "preload": preload,
                                "description": html.escape(description), "canonical": canonical or CONF["site_url"] + rel,
                                "og_title": html.escape(og_title or title), "site_url": CONF["site_url"]}, "head")

    def page(self, rel, title, description, body, root, section="", head_class="", body_class="", css=(), js=(),
             base_tag="", og_title=None, inline="", assets=None, canonical=None, header=None, icon=None):
        """assets: where the page's CSS and scripts are, if not under root (the River's own); header: a header of its
        own, filled (the River's), instead of the site's"""
        v = self.common(root, section)
        a = root if assets is None else assets
        body = quiet_discs(nobreak(fill(body, v, rel or "index")))
        return fill(self.base, {
            "base_tag": base_tag, "head": self.head_html(root, rel, title, description, og_title, canonical, assets, icon),
            "css": "\n".join(f'<link rel="stylesheet" href="{asset(a, c)}">' for c in ("css/base.css", *css)),
            "scripts": inline + "\n".join(f'<script src="{asset(a, j)}" defer></script>' for j in ("js/site.js", *js)),
            "header": header if header is not None else fill(self.header, {**v, "head_class": head_class}, "header"),
            "footer": nobreak(fill(self.footer, v, "footer")), "body_class": body_class,
            "body": fill(body, v, rel or "index"),
        }, rel or "index")


def guide_nav(root, current):
    groups, out = [], []
    for i, g in enumerate(CONF["guide"]):
        if not groups or groups[-1][0] != g["group"]:
            groups.append((g["group"], []))
        groups[-1][1].append((i + 1, g))
    cur = ' aria-current="page"' if current == "" else ""
    out.append(f'<a class="g-home" href="{root}guide/"{cur}><span class="n">00</span>Overview</a>')
    for name, items in groups:
        out.append(f'<p class="label g-group">{html.escape(name)}</p><ol role="list">')
        for n, g in items:
            cur = ' aria-current="page"' if g["slug"] == current else ""
            out.append(f'<li><a href="{root}guide/{g["slug"]}/"{cur}><span class="n">{n:02d}</span>'
                       f'{html.escape(g["title"])}</a></li>')
        out.append("</ol>")
    return "\n".join(out)


def build_guide(site):
    order = CONF["guide"]
    total = len(order)
    pages = [("", None)] + [(g["slug"], g) for g in order]
    for idx, (slug_, g) in enumerate(pages):
        src = SRC / "pages" / "guide" / (f"{slug_}.html" if slug_ else "index.html")
        meta, body = read_page(src)
        root = "../../" if slug_ else "../"
        rel = f"guide/{slug_}/" if slug_ else "guide/"
        body = fill(body, site.common(root, "guide"), src.name)
        body = label_tables(capsify('<div data-caps>' + body + '</div>')[len('<div data-caps>'):-len('</div>')])
        body, toc = anchor_headings(body)
        title = meta.get("title", g["title"] if g else "Guide")
        prev_ = pages[idx - 1] if idx > 0 else None
        next_ = pages[idx + 1] if idx + 1 < len(pages) else None

        def pn(p, cls, word):
            if not p:
                return '<span></span>'
            s, gg = p
            href = f"{root}guide/{s}/" if s else f"{root}guide/"
            n = f"{[x['slug'] for x in order].index(s) + 1:02d}" if s else "00"
            name = gg["title"] if gg else "Overview"
            return (f'<a class="g-pn-{cls}" href="{href}" rel="{"prev" if cls == "prev" else "next"}">'
                    f'<span class="label">{word} · {n}</span><span class="t">{html.escape(name)}</span></a>')

        toc_html = ""
        if len(toc) > 1:
            items = "".join(f'<li><a href="#{i}">{html.escape(html.unescape(t))}</a></li>' for i, t in toc)
            toc_html = (f'<nav class="g-toc" aria-labelledby="toc-h"><p class="label" id="toc-h">On this page</p>'
                        f'<ol role="list">{items}</ol></nav>')
        num = f"{idx:02d}"
        hint = meta.get("hint", "")
        ctl = ""
        if meta.get("buttons"):
            ctl = (f'<div class="g-ctl" role="group" aria-label="On the FM-1"><div class="dhead-btns">{keys_html(meta["buttons"])}</div>'
                   + (f'<p class="dhead-hint">{html.escape(hint)}</p>' if hint else "") + "</div>")
        cur_title = html.escape(g["title"]) if g else "Overview"
        lede = capsify(f'<span data-caps>{meta.get("lede", "")}</span>')
        art = fill(site.guide_tpl, {
            "root": root, "nav": guide_nav(root, slug_), "num": num, "total": f"{total:02d}",
            "h1": meta.get("h1", html.escape(title)), "lede": lede, "ctl": ctl, "toc": toc_html,
            "content": body, "prev": pn(prev_, "prev", "Previous"), "next": pn(next_, "next", "Next"),
            "cur_title": cur_title, "h1_class": " caps" if meta.get("caps") == "yes" else "",
        }, rel)
        desc = meta.get("description") or re.sub(r"<[^>]+>", "", meta.get("lede", ""))
        page_title = f"{html.unescape(re.sub(r'<[^>]+>', '', title))} · FRASIL guide" if slug_ else "Guide · FRASIL"
        out = site.page(rel, page_title, html.unescape(desc), art, root, section="install" if slug_ == "install" else "guide",
                        body_class="guide-page", css=("css/guide.css", "css/widgets.css"),
                        js=("js/harmony.js",) if slug_ == "harmony" else ())
        dst = OUT / rel / "index.html"
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_text(out, encoding="utf-8")
    return len(pages)


# ================================================================ the play page: the emulator, in the site
def replace_once(s, pattern, repl, what, flags=0, keep=False):
    """the pattern, once; with keep, what it matched stays and repl follows it"""
    new, n = re.subn(pattern, (lambda m: m.group(0) + repl) if keep else (lambda m: repl), s, count=1, flags=flags)
    if n != 1:
        die(f"the emulator's page changed ({what} not found): update website/assemble.py's play_page")
    return new


def play_page(site, geo):
    emu = REPO / "app" / "build" / "emu"
    dst = OUT / "play"
    dst.mkdir(parents=True, exist_ok=True)
    meta, parts = read_page(SRC / "pages" / "play.html")
    intro, notes = parts.split("<!-- the emulator -->", 1)
    root = "../"
    v = site.common(root, "play")
    title = meta["title"]
    desc = meta["description"]
    if not (emu / "app.wasm").exists() or not (emu / "index.html").exists():
        if not NO_EMU:
            die("no emulator in app/build/emu: run ./build.sh --web first (or --no-emu for a site without it)")
        print("website: --no-emu: the play page says the emulator is not in this build")
        body = fill(intro, v, "play") + ('<section class="wrap play-missing"><p class="lede">The emulator is not in '
                                         'this build of the site. Build it with <code>./build.sh --web</code>, then '
                                         'run <code>tools/website.sh</code> again.</p></section>') + fill(notes, v, "play")
        (dst / "index.html").write_text(site.page("play/", title, desc, '<main id="main" class="play">' + body + "</main>",
                                                  root, section="play", body_class="play-page",
                                                  css=("css/play.css",)), encoding="utf-8")
        return False
    for f in emu.iterdir():                # the emulator's files, but its build's objects
        if f.name in ("index.html", "o2") or f.suffix == ".o":
            continue
        (shutil.copytree if f.is_dir() else shutil.copy2)(f, dst / f.name)
    words = REPO / "app" / "web" / "words"        # the screen reader's words, where the page looks: ../words/
    if words.is_dir():
        shutil.copytree(words, OUT / "words", dirs_exist_ok=True)
    page = (emu / "index.html").read_text(encoding="utf-8")
    css = "\n".join(f'<link rel="stylesheet" href="{asset(root, c)}">' for c in ("css/base.css", "css/play.css"))
    page = replace_once(page, r'<html lang="en">', '<html lang="en" data-theme="dark">', '<html lang="en">')
    page = replace_once(page, r"<title>.*?</title>\s*", "", "<title>", re.S)
    # its fonts: ours carry the face its panel draws with (Barlow Semi Condensed), and load without blocking
    page = re.sub(r'<link rel="stylesheet" href="https://fonts\.googleapis\.com/[^"]*">\s*', "", page, count=1)
    page = re.sub(r'<link rel="preconnect" href="https://fonts\.(googleapis|gstatic)\.com"[^>]*>\s*', "", page)   # (ours: no other site)
    page = replace_once(page, r'<meta name="description"[^>]*>\s*', "", "the description")
    page = replace_once(page, r'<meta name="viewport"[^>]*>', '\n<script>document.documentElement.classList.add("js")</script>\n'
                        + site.head_html(root, "play/", title, desc), "the viewport", keep=True)
    page = replace_once(page, r"</head>", css + "\n</head>", "</head>")
    page = replace_once(page, r"<body>", '<body class="play-page">\n<a class="skip" href="#main">Skip to content</a>', "<body>")
    page = replace_once(page, r"<header>\s*<h1>[^<]*</h1>\s*</header>",
                        fill(site.header, {**v, "head_class": ""}, "header"), "its header")
    intro, notes = quiet_discs(nobreak(capsify(intro))), quiet_discs(nobreak(capsify(notes)))
    page = replace_once(page, r"<main>", '<main id="main" class="play">\n' + fill(intro, v, "play") +
                        '\n<div class="play-device wrap">', "<main>")
    page = replace_once(page, r"</main>", "</div>\n" + fill(notes, v, "play") + "\n</main>\n" +
                        nobreak(fill(site.footer, v, "footer")), "</main>")
    page = replace_once(page, r"</body>", f'<script src="{asset(root, "js/site.js")}" defer></script>\n'
                        f'<script src="{asset(root, "js/play.js")}" defer></script>\n</body>', "</body>")
    ev = re.search(r'content="([0-9a-f]+)"', engine_meta())   # the engine asked for by its version (as river-player.js)
    for f in ("fetch(\"app.wasm\")", "addModule(\"app_worklet.js\")", "addModule(\"worklet.js\")"):
        if f not in page:
            die(f"the emulator's page no longer has {f}: update website/assemble.py's play_page (its engine's version)")
        if ev:
            page = page.replace(f, f.replace('")', f'?v={ev.group(1)}")'))
    (dst / "index.html").write_text(page, encoding="utf-8")
    return True


# ================================================================ the backup page: fm1-base's protocol, FRASIL's cards
def river_example():
    """the River's example backup (website/river/example.txt on the simulator): example.frasil, beside the page"""
    host = REPO / "app" / "build" / "host" / "frasil_host"
    work = OUT_PARENT / "schema"
    work.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(host), str(SRC / "river" / "example.txt"), str(work)], check=True, capture_output=True)
    shutil.copy2(work / "example.frasil", RIVER / "example.frasil")


@functools.lru_cache(maxsize=None)
def backup_schema():
    """the schema the firmware's own code writes (the simulator's "schema FILE": app.c schema_write), checked, its
    value words shared between the settings that have the same ones"""
    host = REPO / "app" / "build" / "host" / "frasil_host"
    if not host.exists():
        die("no simulator in app/build/host: run ./build.sh --host first (the backup page's schema comes from it)")
    work = OUT_PARENT / "schema"
    work.mkdir(parents=True, exist_ok=True)
    (work / "schema.txt").write_text(f"schema {work / 'schema.json'}\n", encoding="utf-8")
    subprocess.run([str(host), str(work / "schema.txt"), str(work)], check=True, capture_output=True)
    s = json.loads((work / "schema.json").read_text(encoding="utf-8"))
    n, st = s["sizes"], s["scene_t"]
    want = st["name_len"] + n["NSOUND"] + 2 + n["SC_CHORDS"] * 5 + n["SC_ARP"] + 1 + n["SC_SEQ"]
    problems = [what for what, ok in (
        ("scene_t's size", st["size"] == want + 2 * n["SC_BSEQ"] + 4 and st["bq"] == want + 2 * n["SC_BSEQ"] and
         st["tempo"] == st["bq"] + 3),   # (the bass's notes, lengths, SEQUENCE, STEPS, TRANSPOSE, the scene's TEMPO after the rest)
        ("scene_t's offsets", (st["name"], st["v"], st["nch"], st["seq"]) ==
         (0, st["name_len"], st["name_len"] + n["NSOUND"], want - n["SC_SEQ"])),
        ("the earlier layouts' names", all(l.get("name", 0) > 0 for l in s["old_scenes"])),
        ("scenes_t's size", s["scenes_t"]["size"] == s["scenes_t"]["s"] + n["NSCENES"] * st["size"]),
        ("the scenes' record and their sequences' (OBJ_SCENES up to GLINT's, OBJ_BSEQ from it)", st["seq"] == st["rec"] and
         s["scenes_t"]["rec"] == s["scenes_t"]["s"] + n["NSCENES"] * st["rec"] and st["rec"] + s["bseqs_t"]["scene"] == st["size"]
         and s["bseqs_t"]["size"] == s["bseqs_t"]["s"] + n["NSCENES"] * s["bseqs_t"]["scene"]),
        ("settings_t's size", s["settings_t"]["size"] == s["settings_t"]["v"] + n["NSOUND"] + 95),   # (95 after the sound: ANIMATIONS, 2.5)
        ("the earlier shards (the rest of their record as this one's to the bass's sequence, then it or some of it, or none)",
         all(x["rec"] - x["nv"] == st["bnote"] - n["NSOUND"] and 0 <= x["size"] - x["rec"] <= st["size"] - st["bnote"]
             for x in s["old_shards"])),
        ("the earlier basses' records", all(0 < x["scene"] and x["at"] + x["scene"] <= s["bseqs_t"]["scene"]
                                            for x in s["old_bseqs"])),
        ("the earlier settings made over (what follows their sound as this one's)",
         all(x["end"] - 4 - x["nv"] <= s["settings_t"]["size"] - s["settings_t"]["octave"] for x in s["old_settings_over"])),
        ("GLINT's RELEASE where SCATTER was (scatter_gone)", len(s["scatter_gone"]["of"]) == 128 and
         s["scatter_gone"]["p"] < n["NSOUND"] and s["scatter_gone"]["decay"] < n["NSOUND"] and
         all(0 <= x <= s["params"][s["scatter_gone"]["p"]]["max"] for x in s["scatter_gone"]["of"])),
        ("the settings' count", len(s["params"]) == n["NPARAM"]),
        ("each setting's words", all(len(p["texts"]) == p["max"] + 1 for p in s["params"])),
        ("the factory scenes", len(s["factory"]) == n["NSCENES"] and all(len(h) == 2 * st["size"] for h in s["factory"])),
        ("the magics", all(len(m) == 4 for m in s["magic"].values())),
    ) if not ok]
    if problems:
        die("the backup page's schema doesn't match what the page expects: " + ", ".join(problems) +
            " (app.c schema_write, website/assets/js/backup-model.js)")
    texts, index = [], {}
    for p in s["params"]:
        key = "\x00".join(p["texts"])
        if key not in index:
            index[key] = len(texts)
            texts.append(p["texts"])
        p["texts"] = index[key]
    s["texts"] = texts
    return s


@functools.lru_cache(maxsize=None)
def engine_meta():
    """the engine's version (app/build/emu: app.wasm and its worklets, hashed), for river-player.js to ask for them by
    it: a cache (Cloudflare's keeps them 4 hours) never hands an engine of before to a page of now"""
    emu = REPO / "app" / "build" / "emu"
    files = [emu / f for f in ("app.wasm", "worklet.js", "app_worklet.js")]
    if not all(f.exists() for f in files):
        return ""
    h = hashlib.sha1(b"".join(f.read_bytes() for f in files)).hexdigest()[:8]
    return f'\n<meta name="frasil-engine" content="{h}">'


def perform_site():
    """FRASIL Perform (perform.frasilsynth.com), its own site: design/perform/live as it is, its paths made its own
    (assets/: its CSS and scripts, the schema, the fonts, the logo; emu/: the engine its DEMO plays), its head (its P,
    its description, the engine's version), served at its domain's root. 0: the files it has"""
    src = REPO / "design" / "perform"
    shutil.rmtree(PERFORM, ignore_errors=True)
    a = PERFORM / "assets"
    files = {"css/perform.css": src / "proto" / "perform.css", "js/perform-schema.js": src / "data" / "perform-schema.js",
             "js/river-player.js": SRC / "assets" / "js" / "river-player.js", "js/river.js": src / "proto" / "river.js",
             "js/logo.js": src / "proto" / "logo.js", "js/show.js": src / "proto" / "show.js", "js/live.js": src / "live" / "live.js",
             "img/logo-print.svg": SRC / "assets" / "img" / "logo-print.svg"}
    for f in ("perform-favicon.svg", "perform-favicon-32.png", "perform-apple-touch-icon.png"):
        files["img/" + f] = OUT / "assets" / "img" / f
    for f in (OUT / "assets" / "fonts").glob("barlow-semi-condensed-*.woff2"):
        files["fonts/" + f.name] = f
    for rel, f in files.items():
        (a / rel).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(f, a / rel)
    shutil.copy2(OUT / "assets" / "img" / "perform-favicon.ico", PERFORM / "favicon.ico")
    css = (a / "css" / "perform.css").read_text(encoding="utf-8").replace("../../../website/assets/fonts/", "../fonts/")
    (a / "css" / "perform.css").write_text(css, encoding="utf-8")
    live = (a / "js" / "live.js").read_text(encoding="utf-8")
    if 'pl.base = "../../../app/build/emu/";' not in live:
        die("Perform's live.js no longer sets pl.base as it did: update website/assemble.py's perform_site")
    (a / "js" / "live.js").write_text(live.replace('pl.base = "../../../app/build/emu/";', 'pl.base = "emu/";'), encoding="utf-8")
    page = (src / "live" / "index.html").read_text(encoding="utf-8")
    subs = {'href="../proto/perform.css"': 'href="assets/css/perform.css"',
            "../../../website/assets/img/logo-print.svg": "assets/img/logo-print.svg",
            'src="../data/perform-schema.js"': 'src="assets/js/perform-schema.js"',
            'src="../../../website/assets/js/river-player.js"': 'src="assets/js/river-player.js"',
            'src="../proto/river.js"': 'src="assets/js/river.js"', 'src="../proto/logo.js"': 'src="assets/js/logo.js"',
            'src="../proto/show.js"': 'src="assets/js/show.js"', 'src="live.js"': 'src="assets/js/live.js"',
            "<title>FRASIL Perform</title>": "<title>FRASIL Perform</title>\n" + (
                '<meta name="description" content="Your FM-1 under FRASIL, full screen: the river very large, and under it '
                'what your hands do as they do it. For a stage or a film. Chrome or Edge.">\n'
                f'<link rel="canonical" href="{html.escape(CONF.get("perform_url", "https://perform.frasilsynth.com/"))}">\n'
                '<link rel="icon" href="assets/img/perform-favicon.svg" type="image/svg+xml">\n'
                '<link rel="icon" href="assets/img/perform-favicon-32.png" sizes="32x32" type="image/png">\n'
                '<link rel="apple-touch-icon" href="assets/img/perform-apple-touch-icon.png">'
                + engine_meta())}
    for k, v in subs.items():
        if k not in page:
            die(f"Perform's page no longer has {k}: update website/assemble.py's perform_site")
        page = page.replace(k, v)
    if re.search(r'(?:src|href|data-src)="\.\./', page):
        die("Perform's page has a path outside its folder: update website/assemble.py's perform_site")
    (PERFORM / "index.html").write_text(page, encoding="utf-8")
    emu = REPO / "app" / "build" / "emu"     # FRASIL's engine in the browser, for DEMO (./build.sh --web)
    if all((emu / f).exists() for f in ("app.wasm", "worklet.js", "app_worklet.js")):
        (PERFORM / "emu").mkdir()
        for f in ("app.wasm", "worklet.js", "app_worklet.js"):
            shutil.copy2(emu / f, PERFORM / "emu" / f)
    elif not NO_EMU:
        die("no emulator in app/build/emu for Perform's DEMO: run ./build.sh --web first (or --no-emu)")
    return sum(1 for f in PERFORM.rglob("*") if f.is_file())


def admin_site():
    """the admin page (admin.frasilsynth.com), its own site: website/admin/ (index.html, admin.css, admin.js), the main
    site's base.css, fonts, mark and favicons; its data from the River's service, /api/admin/ on its origin (Caddy adds
    the key after its basic auth: server/admin.py). Never indexed. -> the files it has"""
    src = SRC / "admin"
    shutil.rmtree(ADMIN, ignore_errors=True)
    a = ADMIN / "assets"
    files = {"css/base.css": OUT / "assets" / "css" / "base.css", "css/admin.css": src / "admin.css",
             "js/admin.js": src / "admin.js", "img/logo-print.svg": OUT / "assets" / "img" / "logo-print.svg"}
    for f in ("favicon.svg", "favicon-32.png"):
        files["img/" + f] = OUT / "assets" / "img" / f
    for f in (OUT / "assets" / "fonts").glob("*.woff2"):
        files["fonts/" + f.name] = f
    for rel, f in files.items():
        (a / rel).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(f, a / rel)
    shutil.copy2(OUT / "assets" / "img" / "favicon.ico", ADMIN / "favicon.ico")
    v = lambda rel: f"assets/{rel}?v={hashlib.sha1((a / rel).read_bytes()).hexdigest()[:8]}"
    page = fill((src / "index.html").read_text(encoding="utf-8"),
                {"base_css": v("css/base.css"), "admin_css": v("css/admin.css"), "admin_js": v("js/admin.js")}, "admin")
    if 'name="robots" content="noindex,nofollow"' not in page:
        die("website/admin/index.html: its robots meta (noindex,nofollow) is gone")
    (ADMIN / "index.html").write_text(page, encoding="utf-8")
    (ADMIN / "robots.txt").write_text("User-agent: *\nDisallow: /\n", encoding="utf-8")
    return len(files) + 3


def factory_tunes(root):
    """the landing's factory scenes (the schema's, as a FRASIL starts with them): a row each, its name, key and tempo in
    the screen's words, and their bytes (scene_t, as river-player.js plays them) for listen.js"""
    s = backup_schema()
    st, n, ids = s["scene_t"], s["sizes"], s["ids"]
    words = lambda p: s["texts"][s["params"][ids[p]]["texts"]]
    rows, data = [], []
    for i, hx in enumerate(s["factory"][:n["NFACTORY"]]):
        b = bytes.fromhex(hx)
        name = b[st["name"]:st["name"] + st["name_len"]].split(b"\0")[0].decode("ascii")
        v = b[st["v"]:st["v"] + n["NSOUND"]]
        key = f'{words("P_ROOT")[v[ids["P_ROOT"]]]} {words("P_SCALE")[v[ids["P_SCALE"]]]}'
        bpm = words("P_TEMPO")[b[st["tempo"]]]
        data.append({"name": name, "bpm": int(bpm.split()[0]), "hex": hx})
        rows.append(f'<li class="tune" data-i="{i}"><button type="button" class="tune-b" aria-pressed="false" '
                    f'aria-label="Play {html.escape(name)}"><span class="tune-play" aria-hidden="true"></span>'
                    f'<span class="tune-name">{html.escape(name)}</span><span class="tune-meta">{html.escape(key)} · '
                    f'{html.escape(bpm)}</span><canvas class="tune-flow" aria-hidden="true"></canvas></button></li>')
    return {"tunes": "\n        ".join(rows), "tunes_emu": root + "play/",
            "tunes_json": json.dumps(data, separators=(",", ":")).replace("</", "<\\/")}


def backup_protocol():
    """fm1-base's backup page's protocol (FW, PB) and its Link, as they are, filled in for FRASIL (make_site.py's
    backup_values): extracted, never rewritten"""
    sys.path.insert(0, str(REPO / "base" / "tools"))
    from make_site import backup_values
    from build import read_conf
    src = (REPO / "base" / "web" / "backup" / "index.html").read_text(encoding="utf-8")
    proto = re.search(r"// --- backup protocol: begin[^\n]*\n.*?// --- backup protocol: end\n", src, re.S)
    link = re.search(r"// one MIDI in/out pair with a SysEx queue.*?\nclass Link \{.*?\n\}\n", src, re.S)
    if not proto or not link:
        die("fm1-base's web/backup/index.html changed (its protocol's markers or its Link): update website/assemble.py's "
            "backup_protocol")
    code = proto.group(0) + "\n" + link.group(0)
    values = backup_values(REPO / "app", read_conf(REPO / "app" / "fm1.toml"))
    for k, v in values.items():
        code = code.replace("{{" + k + "}}", v)
    left = sorted(set(re.findall(r"\{\{[A-Z_]+\}\}", code)))
    if left:
        die(f"the backup protocol: {' '.join(left)} not filled in")
    return code, values


RIVER_PAGES = (   # the River's pages (website/pages/river/): where each is served, its own script, the header's part lit
    ("navigate", "", "js/river-navigate.js", "community"),   # the Community River, its front page
    ("yours", "yours/", "js/river-yours.js", "yours"),       # Your Private River (signed out: signing in)
    ("tools", "tools/", "js/backup.js", "tools"),            # Tools: Quick Backup, its one tool
    ("tools", "backup/", "js/backup.js", "tools"),           # (Quick Backup's own address, as before: the same page)
    ("profile", "profile/", "js/river-profile.js", "profile"),   # the account, the header's name signed in
    ("privacy", "privacy/", None, ""),                       # (privacy and terms)
)
RIVER_JS = ("js/backup-model.js", "js/river-player.js", "js/river-cards.js", "js/river-keys.js", "js/river-api.js", "js/river-device.js",
            "js/river-list.js", "js/river-edit.js", "js/river-head.js")
def river_page(site):
    """FRASIL River (river.frasilsynth.com), its own site: its own header (templates/river-header.html: its name,
    then Your Private River, the Community River (its front page), Tools, and who is signed in), its pages (RIVER_PAGES);
    their CSS and scripts at its root, the footer and the images the main site's. Each page carries fm1-base's
    protocol and the schema (any of them talks to the FM-1)"""
    if RIVER.exists():
        shutil.rmtree(RIVER)
    RIVER.mkdir(parents=True)
    code, values = backup_protocol()
    schema = json.dumps(backup_schema(), separators=(",", ":")).replace("</", "<\\/")
    version = (REPO / "VERSION").read_text(encoding="utf-8").strip()
    inline = (f'<script>\n"use strict";\n{code}\nconst SCHEMA = {schema};\nconst FRASIL_VERSION = "{version}";\n'
              f'</script>\n')
    root = CONF["site_url"]
    css = ("css/river.css", "css/river-head.css")
    head_tpl = (SRC / "templates" / "river-header.html").read_text(encoding="utf-8")
    fonts = [f"fonts/{f.name}" for f in sorted((OUT / "assets" / "fonts").glob("*.woff2"))]
    icons = ("img/river-favicon.svg", "img/river-favicon-32.png", "img/river-apple-touch-icon.png",   # (its R; Perform's
             "img/perform-favicon.svg")                                                                 # P in the menu)
    for f in ("css/base.css", *css, "js/site.js", *RIVER_JS, *(p[2] for p in RIVER_PAGES if p[2]), "img/logo-duo.png", *icons, *fonts):   # (the logo's FRASIL; the fonts)
        (RIVER / "assets" / f).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(OUT / "assets" / f, RIVER / "assets" / f)
    shutil.copy2(OUT / "assets" / "img" / "river-favicon.ico", RIVER / "favicon.ico")
    river_example()
    emu = REPO / "app" / "build" / "emu"   # FRASIL's engine in the browser, to play a shard (app/web.c, ./build.sh --web)
    if all((emu / f).exists() for f in ("app.wasm", "worklet.js", "app_worklet.js")):
        (RIVER / "emu").mkdir()
        for f in ("app.wasm", "worklet.js", "app_worklet.js"):
            shutil.copy2(emu / f, RIVER / "emu" / f)
    elif not NO_EMU:
        die("no emulator in app/build/emu for the River's ▶: run ./build.sh --web first (or --no-emu)")
    for name, rel, js, part in RIVER_PAGES:
        meta, body = read_page(SRC / "pages" / "river" / f"{name}.html")
        body = capsify(fill(body, {**site.common(root, "the-river"), "ext": values["EXT"], "site": root}, f"river/{name}"))
        header = fill(head_tpl, {"root": root, **discord_links(), "menu": menu_html(CONF["site_url"], "/assets/img/"), **{f"cur_{k}": ' aria-current="page"' if k == part else ""
                                                  for k in ("yours", "community", "tools", "profile")}}, "river-header")
        out = RIVER / rel / "index.html"
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(site.page(rel, meta["title"], meta["description"], body, root, section="the-river",
                                 body_class=f"river-page river-{name}", css=css, js=(*RIVER_JS, *([js] if js else [])),
                                 inline=inline,
                                 assets="../" * rel.count("/") or "./", canonical=CONF["river_url"] + rel, header=header,
                                 icon="river-"),
                       encoding="utf-8")
    return len(schema)


# ================================================================ links
class Links(HTMLParser):
    def __init__(self):
        super().__init__()
        self.refs, self.ids, self.base = [], set(), None

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if "id" in a:
            self.ids.add(a["id"])
        if tag == "base" and a.get("href"):
            self.base = a["href"]
        for k in ("href", "src"):
            if a.get(k):
                self.refs.append((tag, a[k]))
        if a.get("srcset"):
            for item in a["srcset"].split(","):
                self.refs.append((tag, item.strip().split(" ")[0]))


def check_links(out=OUT, serve=SERVE, service=()):
    """every page's links lead somewhere (service: paths answered by a service, not by files: the River's /api/)"""
    pages, problems = {}, []
    for f in out.rglob("*.html"):
        p = Links()
        p.feed(f.read_text(encoding="utf-8"))
        pages[f] = p
    for f, p in pages.items():
        rel = f.parent.relative_to(out).as_posix()
        url_dir = p.base or serve + ("" if rel == "." else rel + "/")
        for tag, ref in p.refs:
            if re.match(r"^(https?:|mailto:|data:|//|javascript:)", ref) or tag == "base":
                continue
            path, frag = urldefrag(ref)
            path = urlsplit(path).path
            if path and posixpath.join(url_dir, path).startswith(service or ("\0",)):
                continue
            target = f
            if path:
                abs_ = posixpath.normpath(posixpath.join(url_dir, path))
                if not (abs_ + "/").startswith(serve):
                    problems.append(f"{f.relative_to(OUT_PARENT)}: {ref} leaves the site")
                    continue
                target = out / (abs_ + "/")[len(serve):].rstrip("/")
                if target.is_dir():
                    target = target / "index.html"
                if not target.exists():
                    problems.append(f"{f.relative_to(OUT_PARENT)}: {ref} -> nothing ({abs_})")
                    continue
            if frag and target.suffix == ".html":
                tp = pages.get(target)
                if tp is not None and frag not in tp.ids:
                    problems.append(f"{f.relative_to(OUT_PARENT)}: {ref}: no #{frag} there")
    return len(pages), problems


# ================================================================ all of it
def main():
    if not (REPO / "app" / "assets" / "frasil-logo.png").exists():
        die("no app/assets/frasil-logo.png: run from FRASIL's repository")
    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True)
    shutil.copytree(SRC / "assets", OUT / "assets")
    for f in (OUT / "assets").rglob("*"):   # (the cheat sheet's files too: a cache keeps them hours, as it does these)
        if f.suffix in (".css", ".js") or f.parent.name == "cheatsheet":
            ASSET_V[str(f.relative_to(OUT / "assets"))] = hashlib.sha1(f.read_bytes()).hexdigest()[:8]
    img = OUT / "assets" / "img"
    img.mkdir(parents=True, exist_ok=True)
    geo = make_images(img)
    n_screens = copy_screens(OUT / "assets" / "screens")
    site = Site()
    v = {**geo, "status": html.escape(CONF["status"])}

    meta, body = read_page(SRC / "pages" / "index.html")
    body = capsify(fill(body, {**v, **site.common("./", "home"), "gallery": gallery("./"), **factory_tunes("./")}, "index"))
    (OUT / "index.html").write_text(site.page("", meta["title"], meta["description"], body, "./", section="home",
                                              head_class=" over", body_class="home", css=("css/landing.css", "css/widgets.css"),
                                              js=("js/river.js", "js/landing.js", "js/harmony.js", "js/river-player.js", "js/listen.js"),
                                              og_title=meta.get("og_title")), encoding="utf-8")
    n_guide = build_guide(site)
    emu = play_page(site, geo)
    schema_size = river_page(site)
    meta, body = read_page(SRC / "pages" / "river-library.html")   # (the main site's word on the River, and the way there)
    (OUT / "river").mkdir()
    (OUT / "river" / "index.html").write_text(site.page("river/", meta["title"], meta["description"], capsify(body), "../",
                                                        section="river", css=("css/river-library.css",)), encoding="utf-8")
    meta, body = read_page(SRC / "pages" / "credits.html")   # (who made what FRASIL is built on and set in)
    (OUT / "credits").mkdir()
    (OUT / "credits" / "index.html").write_text(site.page("credits/", meta["title"], meta["description"], capsify(body), "../",
                                                          css=("css/river-library.css",)), encoding="utf-8")
    jump, notes = changelog_html()       # (CHANGELOG.md, as it is in the repo: every version, the newest first)
    (OUT / "changelog").mkdir()
    (OUT / "changelog" / "index.html").write_text(site.page(
        "changelog/", "Changelog · FRASIL", "What each FRASIL release brought, newest first.", capsify(
            '<main id="main"><section class="rl wrap cl" aria-labelledby="cl-title">'
            '<div class="dhead"><p class="dhead-name">Changelog</p></div>'
            '<h1 class="display sm" id="cl-title">Every release, <em>newest first.</em></h1>'
            f'<nav class="cl-jump" aria-label="Versions">{jump}</nav><div data-caps>{notes}</div></section></main>'), "../",
        css=("css/river-library.css", "css/changelog.css")), encoding="utf-8")
    meta, body = read_page(SRC / "pages" / "cheatsheet.html")
    (OUT / "cheatsheet").mkdir()
    (OUT / "cheatsheet" / "index.html").write_text(site.page("cheatsheet/", meta["title"], meta["description"], capsify(body),
                                                             "../", section="cheat", body_class="cheatsheet-page",
                                                             css=("css/cheatsheet.css",)), encoding="utf-8")
    meta, body = read_page(SRC / "pages" / "404.html")
    (OUT / "404.html").write_text(site.page("404.html", meta["title"], meta["description"], body, CONF["base_path"],
                                            body_class="lost", css=("css/landing.css",)), encoding="utf-8")
    (OUT / ".nojekyll").write_text("")
    river_a = re.compile(r'<a (?![^>]*\btarget=)([^>]*\bhref="' + re.escape(CONF["river_url"]) + r'[^"]*")')
    for f in OUT.rglob("*.html"):        # the River, from the main site: in a tab of its own
        s = f.read_text(encoding="utf-8")
        t = river_a.sub(r'<a target="_blank" rel="noopener" \1', s)
        if t != s:
            f.write_text(t, encoding="utf-8")
    n, problems = check_links()
    n_river, river_problems = check_links(RIVER, "/", service=("/api/",))
    problems += river_problems
    for p in problems:
        print("  broken  " + p)
    if problems:
        die(f"{len(problems)} broken link(s)")
    size = sum(f.stat().st_size for f in OUT.rglob("*") if f.is_file())
    print(f"website  {OUT}  {n} pages (landing, {n_guide} guide, play{'' if emu else ' without the emulator'}, River library, credits, changelog, cheat sheet, 404), "
          f"{n_screens} screenshots, {size / 1e6:.1f} MB; links ok")
    n_perform = perform_site()
    print(f"perform  {PERFORM}  Perform ({n_perform} files)")
    print(f"river    {RIVER}  the River ({n_river} page{'s' if n_river != 1 else ''}, its schema {schema_size / 1e3:.0f} kB); links ok")
    n_admin = admin_site()
    n_pages, admin_problems = check_links(ADMIN, "/", service=("/api/",))
    for p in admin_problems:
        print("  broken  " + p)
    if admin_problems:
        die(f"{len(admin_problems)} broken link(s) in the admin page")
    print(f"admin    {ADMIN}  the admin page ({n_pages} page, {n_admin} files); links ok")


if __name__ == "__main__":
    main()
