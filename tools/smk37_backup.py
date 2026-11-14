#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""smk37_backup: an SMK-37's whole flash, read twice and kept, before FRASIL goes on it (card #111). It never writes
the keyboard's flash. The procedure, step by step: docs/SMK37.md.

  .venv/bin/python tools/smk37_backup.py uboot frasil-smk37-elite-X.Y.Z.fwsc --port SMK-37     (the keyboard's model's:
      -elite, -pro or -mke; another model's is refused)
      the stock firmware checks FRASIL's package (an install's first step: its firmware area is not written) and
      restarts into FRASIL's update loader, which is sent the UBOOT key: the chip's own update mode (WL82 UBOOT1.00,
      a USB disk)
  sudo .venv/bin/python tools/smk37_backup.py read --model elite     (first, in another terminal: it waits)
      JieLi's flash loader into the chip's RAM (fm1_rescue's: downloaded, checked by its hash), the chip and its flash
      checked, the 1 MiB read twice, the two the same, the factory calibration's mark (trimdat at 0xC1224) there; saved
      in ~/frasil-backups/smk37-MODEL/ (0600, a .sha256 beside it), never in the repository: the unit's own key, MAC,
      pairings and calibration are in it

The chip's update mode and the reads are fm1-base's felucca-bsp tools/fm1_rescue.py (its USB, its UBOOT commands); the
install's check step is fm1-base's tools/fm1_install.py. Only their reading parts are used here.
"""
import argparse
import hashlib
import os
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "base", "bsp", "tools"))
sys.path.insert(0, os.path.join(ROOT, "base", "tools"))   # (first: fm1-base's fm1_install, not felucca-bsp's)
import fm1_rescue as r  # noqa: E402

UBOOT_KEY = bytes([0xF0, 0x22, 0x24, 0x35, 0x7D, 0xF7])   # the loader's soft key: the mask-ROM UBOOT
# the SMK-37's flash parts (felucca-bsp boards/smk37/fm1_flash.h FL_JEDEC_OK): the tested unit's, and the FM-1's two
SMK_FLASH = {0xEB6014: "EB6014 (the SMK-37's)", 0x856014: "Puya P25Q80", 0xB36014: "UC25HQ80"}
TRIM_AT, TRIM = 0xC1224, b"trimdat\0"                   # the end of the factory calibration (USRTRIM, 0xC1000)
MODELS = ("pro", "elite", "mke")


def to_uboot(pkg, port=None, out=sys.stdout, ask=input):
    """the install's check step with FRASIL's SMK-37 package, then the UBOOT key to its loader"""
    import fm1_install as fi
    product, image, _ = fi.load_package(pkg, False)
    if not product.startswith(("SMK-37 ", "MKE-P37")):
        raise r.Fail(f"{pkg}: not an SMK-37 package ({product}): FM1B_BOARD=smk37 FM1B_MODEL=elite ./build.sh")
    up = fi.Updater(fi.MidoBackend(), port)
    dev = up.find()
    if not dev:
        raise r.Fail("no SMK-37 on USB MIDI: plug it in, switch it on, close every app that uses its MIDI port")
    print(f"keyboard: {dev.id.text}  ({dev.name})", file=out)
    if fi.model_of(dev.id.text) != fi.model_of(product):
        dev.link.close()
        raise r.Fail(f"the keyboard is {dev.id.text}, the package is for {product}: nothing done")
    if not dev.id.loader:
        if ask("It checks FRASIL's package and restarts into FRASIL's update loader; nothing of its firmware is "
               "written. Go on? [y/N] ").strip().lower() not in ("y", "yes"):
            dev.link.close()
            print("nothing done", file=out)
            return 1
        prog = fi.Progress(out)
        try:
            dev = up.check(dev, image, prog)
        finally:
            prog.end()
    dev.link.send(UBOOT_KEY)
    dev.link.close()
    print("the UBOOT key sent: the keyboard is in the chip's update mode (WL82 UBOOT1.00, a USB disk; eject nothing).\n"
          "the read takes it: sudo .venv/bin/python tools/smk37_backup.py read --model MODEL (started first, in another "
          "terminal; docs/SMK37.md)", file=out)
    return 0


def read_twice(dev, loader, say=print):
    """the whole flash, twice, from the chip's update mode -> (image, flash id, trimdat there); reads only"""
    vendor, product = dev.inquiry()
    say(f"  device: {vendor} {product}")
    if vendor != "WL82":
        raise r.Fail(f"not a WL82 UBOOT ({vendor!r}): nothing read")
    dev.start_loader(loader)
    key, typ, fid = dev.chip()
    say(f"  chip key {key:04X}, flash type {typ}, id {fid:06X} ({SMK_FLASH.get(fid, 'unknown')})")
    if (key, typ) != (0x980F, 3) or fid not in SMK_FLASH:
        raise r.Fail("not the SMK-37's chip and flash (key 980F, type 3, id EB6014, 856014 or B36014): nothing read")
    images = []
    for n in (1, 2):
        t0 = time.time()
        images.append(dev.read(0, r.FLASH_SIZE))
        say(f"  read {n}: {len(images[-1])} B in {time.time() - t0:.1f} s, sha256 "
            f"{hashlib.sha256(images[-1]).hexdigest()[:16]}")
    a, b = images
    if len(a) != r.FLASH_SIZE or a != b:
        raise r.Fail("the two reads differ: nothing kept; read again")
    return a, fid, a[TRIM_AT:TRIM_AT + len(TRIM)] == TRIM


def owner_home():
    """the home of whoever ran sudo (the files are theirs), else this user's"""
    user = os.environ.get("SUDO_USER")
    if user:
        import pwd
        return pwd.getpwnam(user).pw_dir
    return os.path.expanduser("~")


def save(img, fid, model, home, trim):
    """img into HOME/frasil-backups/smk37-MODEL/, 0600, and its .sha256 -> the path"""
    d = os.path.join(home, "frasil-backups", f"smk37-{model}")
    os.makedirs(d, mode=0o700, exist_ok=True)
    name = time.strftime(f"smk37-{model}-%Y%m%d-%H%M%S") + ("" if trim else "-no-trimdat")
    path = os.path.join(d, name + ".bin")
    sha = hashlib.sha256(img).hexdigest()
    for p, data in ((path, img), (path + ".sha256", f"{sha}  {name}.bin  flash {fid:06X}\n".encode())):
        fd = os.open(p, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, "wb") as f:
            f.write(data)
    uid, gid = os.environ.get("SUDO_UID"), os.environ.get("SUDO_GID")
    if uid and gid:
        for p in (d, os.path.dirname(d), path, path + ".sha256"):
            os.chown(p, int(uid), int(gid))
    return path, sha


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="step", required=True)
    u = sub.add_parser("uboot", help="the install's check step, then the UBOOT key (USB MIDI)")
    u.add_argument("package", help="FRASIL's SMK-37 package, the keyboard's model's (frasil-smk37-elite|pro|mke-X.Y.Z.fwsc)")
    u.add_argument("--port", metavar="NAME", help="the MIDI port (part of its name)")
    rd = sub.add_parser("read", help="the whole flash, read twice, kept (sudo)")
    rd.add_argument("--model", choices=MODELS, required=True, help="the keyboard's model, for the file's name")
    rd.add_argument("--loader", help="a local wl82loader.bin instead of downloading it")
    rd.add_argument("--wait", type=float, default=120, help="seconds to wait for the chip's update mode (120)")
    a = ap.parse_args(argv)
    try:
        if a.step == "uboot":
            return to_uboot(a.package, a.port)
        loader = r.get_loader(a.loader)
        if os.geteuid() != 0:
            raise r.Fail("run it with sudo: the system has to give the device up")
        try:
            import usb.core
            import usb.util
        except ImportError:
            raise r.Fail("pyusb is missing: .venv/bin/pip install pyusb libusb-package (docs/SMK37.md)")
        bot = r.open_device(usb, a.wait)
        try:
            img, fid, trim = read_twice(r.Uboot(bot), loader, say=lambda *x, **k: print(*x, **k, flush=True))
        finally:
            try:
                usb.util.dispose_resources(bot.dev)
            except Exception:
                pass
        path, sha = save(img, fid, a.model, owner_home(), trim)
        print(f"  kept: {path}\n  sha256 {sha}")
        if not trim:
            print("  no trimdat at 0xC1224: the calibration's mark is not there (read again; keep this one too)")
            return 1
        print("  the calibration's mark is there. Copy the file to a second place (never into a repository).\n"
              "  Switch the keyboard off and on: it starts in FRASIL's update loader (the install's record waits).")
        return 0
    except r.Fail as e:
        print(f"smk37_backup: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
