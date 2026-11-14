#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""tools/smk37_backup.py's read against a simulated SMK-37 in the chip's update mode: a good flash kept (0600, its
sha256 beside it), two reads that differ or another chip kept nowhere, a missing calibration mark kept and said, and
not one erase or write asked of the keyboard.  .venv/bin/python tools/smk37_backup_test.py"""
import hashlib
import os
import random
import stat
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smk37_backup as b  # noqa: E402

r = b.r


class FakeSMK(r.Uboot):
    """the UBOOT loader's reads on a 1 MiB bytearray; any write or erase fails the test"""

    def __init__(self, flash, fid=0xEB6014, flaky=False):
        self.flash, self.fid, self.flaky, self.reads = bytearray(flash), fid, flaky, 0

    def inquiry(self):
        return "WL82", "UBOOT1.00"

    def start_loader(self, blob):
        pass

    def chip(self):
        return 0x980F, 3, self.fid

    def read(self, addr, n):
        self.reads += 1
        d = bytearray(self.flash[addr:addr + n])
        if self.flaky and self.reads == 2:
            d[0x50000] ^= 1                         # (a bit read wrong the second time)
        return bytes(d)

    def cmd(self, cmd, args=b""):
        raise AssertionError(f"a command to the keyboard: {cmd}")

    def write_block(self, cmd, addr, data):
        raise AssertionError("a write to the keyboard")

    def write_sector(self, addr, data, end):
        raise AssertionError("a write to the keyboard")

    def run_app(self):
        raise AssertionError("the app started")


def flash(trim=True):
    rnd = random.Random(37)
    f = bytearray(rnd.getrandbits(8) for _ in range(r.FLASH_SIZE))
    if trim:
        f[b.TRIM_AT:b.TRIM_AT + len(b.TRIM)] = b.TRIM
    return bytes(f)


def fails(fn, text):
    try:
        fn()
    except r.Fail as e:
        assert text in str(e), e
        return
    raise AssertionError(f"no Fail ({text})")


def main():
    quiet = lambda *a, **k: None   # noqa: E731
    good = flash()
    img, fid, trim = b.read_twice(FakeSMK(good), b"", quiet)
    assert img == good and fid == 0xEB6014 and trim
    with tempfile.TemporaryDirectory() as home:
        path, sha = b.save(img, fid, "elite", home, trim)
        assert os.path.dirname(path) == os.path.join(home, "frasil-backups", "smk37-elite")
        assert open(path, "rb").read() == good and sha == hashlib.sha256(good).hexdigest()
        assert stat.S_IMODE(os.stat(path).st_mode) == 0o600
        assert stat.S_IMODE(os.stat(os.path.dirname(path)).st_mode) == 0o700
        assert open(path + ".sha256").read().startswith(sha + "  ") and "EB6014" in open(path + ".sha256").read()
        img2, fid2, trim2 = b.read_twice(FakeSMK(flash(trim=False)), b"", quiet)
        assert not trim2
        p2, _ = b.save(img2, fid2, "elite", home, trim2)
        assert p2.endswith("-no-trimdat.bin")
    print("a good flash: read twice, kept 0600 with its sha256                ok")
    fails(lambda: b.read_twice(FakeSMK(good, flaky=True), b"", quiet), "the two reads differ")
    print("two reads that differ: nothing kept                               ok")
    fails(lambda: b.read_twice(FakeSMK(good, fid=0xEF4014), b"", quiet), "nothing read")
    print("another flash part: nothing read                                  ok")
    for fid in (0x856014, 0xB36014):
        assert b.read_twice(FakeSMK(good, fid=fid), b"", quiet)[1] == fid
    print("the FM-1's two parts, listed for the SMK-37 too: read            ok")
    print("no erase, no write, no start asked of the keyboard                ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
