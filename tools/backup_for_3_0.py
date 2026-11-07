#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""A FRASIL backup (.frasil, "FRBK") made by 3.0.1 or later, as FRASIL 3.0 (or 3.0.1) restores it.

3.0 refuses a backup with more objects than it has (fm1-base's backup.c: the count past its own). 3.0.1 adds one, OBJ_NAMES
(id 6): the scenes' names past their sixteenth character. This writes a copy with the objects 3.0 knows alone (ids 0 .. 5:
the settings, the scenes, their bass and drone sequences, the player's presets), each as it was, the count and the CRCs
made again. On 3.0 every scene then has the first sixteen characters of its name; nothing else changes.

3.0.2 (LOFI TAPE II) writes two of them in layouts 3.0 and 3.0.1 do not read: the settings "FRSQ" (3.0's "FRSP" and the
four's) and the drone sequences "FRDR" (each scene's "FRDQ" and the four's). They are written back as 3.0 had them, the
four left out (3.0 has none: its tape as it was). For 3.0 the screensaver's byte too: IDLE TIME (3.0.1's, 2 ..
121 in it) read by 3.0 as OFF, written 0 (ON) or 1 (OFF) as it says, its minutes 3.0's ten. --3.0.1 keeps OBJ_NAMES and
IDLE TIME (3.0.1 reads them).

Then the LFOs' AMOUNTs and SWELL, kept in the scene: the settings "FRSS" (FRSQ's and the twelve and SWELL after it) and
OBJ_NAMES "FRNO" (each slot's sum and the name's rest, then its twelve AMOUNTs and SWELL); "FRSR" and "FRNN" (3.0.2-b5's
test builds) the same bytes, their AMOUNTs in percent. The settings go back as above; for 3.0.1, OBJ_NAMES goes back to
"FRNM", each slot's sum and the name's rest alone (3.0.1 has no AMOUNT: every LFO as it moved, SWELL not kept).

    tools/backup_for_3_0.py [--3.0.1] IN.frasil OUT.frasil
"""
import struct
import sys
import zlib

MAGIC, HEAD, KNOWN = b"FRBK", 16, 6       # ids below 6: 3.0's (app.h APP_BACKUP_OBJS)
OBJ_SETTINGS, OBJ_DSEQ, OBJ_NAMES = 0, 4, 6
FRSP_BYTES, SAVER_AT = 280, 279           # app.c: FRSP's record, its last byte SCREENSAVER's (settings_t.saver_off)
FRSQ_BYTES = FRSP_BYTES + 4               # and LOFI TAPE II's four after it
FRSS_BYTES = 300                          # and the twelve AMOUNTs and SWELL after them (settings_t, padded: FRSS, FRSR)
NMS_FRNM, NMS_FRNO = 4 + 16, 4 + 16 + 13  # a slot's in OBJ_NAMES: FRNM's (3.0.1: the sum, the name's rest), FRNO's, FRNN's
NSCENES, DSQ_FRDQ, DSQ_FRDR = 16, 230, 234   # a scene's in OBJ_DSEQ: FRDQ's (3.0), FRDR's (3.0.2: and the four)


def objects(image):
    if len(image) < HEAD + 4 or image[:4] != MAGIC or struct.unpack_from("<I", image, 12)[0] != zlib.crc32(image[:12]):
        raise ValueError("not a FRASIL backup (FRBK), or its head is damaged")
    if struct.unpack_from("<I", image, len(image) - 4)[0] != zlib.crc32(image[:-4]):
        raise ValueError("this backup is damaged (its CRC)")
    version, n, flags = struct.unpack_from("<HHI", image, 4)
    pos, out = HEAD, []
    for _ in range(n):
        obj, ln, crc = struct.unpack_from("<HHI", image, pos)
        data = image[pos + 8:pos + 8 + ln]
        if len(data) != ln or zlib.crc32(data) != crc:
            raise ValueError(f"this backup is damaged (object {obj})")
        out.append((obj, data))
        pos += 8 + ln
    return version, flags, out


def build(version, flags, objs):
    head = MAGIC + struct.pack("<HHI", version, len(objs), flags)
    body = head + struct.pack("<I", zlib.crc32(head))
    for obj, data in objs:
        body += struct.pack("<HHI", obj, len(data), zlib.crc32(data)) + data
    return body + struct.pack("<I", zlib.crc32(body))


def settings_for(data, for_301):
    """FRSS's, FRSR's and FRSQ's as FRSP; for 3.0, SCREENSAVER's byte as 3.0 reads it (0 ON, 1 OFF)"""
    if (len(data), data[:4]) in ((FRSQ_BYTES, b"FRSQ"), (FRSS_BYTES, b"FRSR"), (FRSS_BYTES, b"FRSS")):
        data = b"FRSP" + data[4:FRSP_BYTES]
    if not for_301 and len(data) == FRSP_BYTES and data[:4] == b"FRSP":
        b = data[SAVER_AT]
        on = b == 0 or 2 <= b <= 61 or b > 121           # (app.c saver_unbyte)
        data = data[:SAVER_AT] + bytes([0 if on else 1])
    return data


def dseq_for(data):
    """FRDR's as FRDQ: each scene's first 230 bytes"""
    if len(data) == 4 + NSCENES * DSQ_FRDR and data[:4] == b"FRDR":
        data = b"FRDQ" + b"".join(data[4 + s * DSQ_FRDR:4 + s * DSQ_FRDR + DSQ_FRDQ] for s in range(NSCENES))
    return data


def names_for(data):
    """FRNO's and FRNN's as FRNM: each slot's first 20 bytes"""
    if len(data) == 4 + NSCENES * NMS_FRNO and data[:4] in (b"FRNO", b"FRNN"):
        data = b"FRNM" + b"".join(data[4 + s * NMS_FRNO:4 + s * NMS_FRNO + NMS_FRNM] for s in range(NSCENES))
    return data


def main(argv):
    for_301 = "--3.0.1" in argv[1:]
    args = [a for a in argv[1:] if a != "--3.0.1"]
    if len(args) != 2:
        print(__doc__.strip().splitlines()[-1].strip(), file=sys.stderr)
        return 2
    try:
        version, flags, objs = objects(open(args[0], "rb").read())
    except (OSError, ValueError) as e:
        print(f"{args[0]}: {e}", file=sys.stderr)
        return 1
    keep = KNOWN + 1 if for_301 else KNOWN
    kept = [(o, settings_for(d, for_301) if o == OBJ_SETTINGS else dseq_for(d) if o == OBJ_DSEQ else
             names_for(d) if o == OBJ_NAMES else d) for o, d in objs if o < keep]
    open(args[1], "wb").write(build(version, flags, kept))
    left = [o for o, _ in objs if o >= keep]
    made = [o for (o, d), (_, d2) in zip([x for x in objs if x[0] < keep], kept) if d != d2]
    print(f"{args[1]}: {len(kept)} objects" + (f" (left out: {', '.join(map(str, left))})" if left else " (nothing left out)") +
          (f", made over: {', '.join(map(str, made))}" if made else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
