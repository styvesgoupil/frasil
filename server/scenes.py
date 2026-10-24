# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""What the River accepts, read with the schema the firmware's own code writes (schema.json, made by make_schema.py
from the simulator: app.c schema_write; never a layout copied by hand).
  Schema(path).shard(data, layout)   a scene_t of that layout (magic.shard: the scenes' record's, then the bass's
                                     sequence) -> its bytes, the name NUL-padded; Invalid if not
  .renamed(data, name)               the same bytes under another name
  .upgraded(data, layout)            a shard of an earlier layout (FRSm: before the scene's TEMPO; FRSk: SCATTER where
                                     GLINT's RELEASE is too; FRSi: before BASS's DECAY, SUSTAIN and PULSE too; FRSh:
                                     before the bass's sequence too; FRSg: names of ten) -> its bytes in this one, as
                                     the FM-1 makes it over (the name NUL-padded, the bass empty, the new settings and
                                     TEMPO at their defaults, RELEASE as DECAY let a glint go then); None if not one
  .derived(data)                     what the gallery filters on: root, scale, type (GLINT's OFF/ARP/SEQ), drone, glint;
                                     bseq (1: the bass's sequence on, its bq_on)
  .backup(image)                     a backup file (fm1-base's: base/tools/fm1_backup.py parse) -> the names of the
                                     scenes saved in it (None: an earlier FRASIL's layout); Invalid if it is damaged
  .pack(shards, firmware)            a scene package (.frasilscenes), as the page's FB.packOf writes it
"""
import base64
import json
import struct
import zlib

NAME_CHARS = frozenset(b"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -.#+")
BACKUP_MAX = 8192
HEAD, REC, PAYLOAD_MAX = 16, 8, 3840                  # (fm1_backup.py's)


class Invalid(ValueError):
    pass


class Schema:
    def __init__(self, path):
        with open(path, encoding="utf-8") as f:
            s = json.load(f)
        if s.get("format") != "frasil-schema" or "backup" not in s:
            raise SystemExit(f"{path}: not the River's schema (python3 server/make_schema.py)")
        self.s, st, n = s, s["scene_t"], s["sizes"]
        self.layout, self.size, self.firmware = s["magic"]["shard"], st["size"], s.get("firmware", "")
        self.rec = st["rec"]                          # (a scene's bytes in the scenes' record: the bass's after them)
        self.name_at, self.name_len, self.v_at, self.nsound = st["name"], st["name_len"], st["v"], n["NSOUND"]
        self.bq = st["bq"]                            # (the bass's sequence: bq_on, bq_steps, bq_trans)
        self.max = [p["max"] for p in s["params"][:self.nsound]]
        self.gone = s["scatter_gone"]                 # (GLINT's RELEASE where every earlier layout kept SCATTER)
        ids = s["ids"]
        self.at = {k: ids[p] for k, p in (("root", "P_ROOT"), ("scale", "P_SCALE"), ("type", "P_SQ_TYPE"),
                                          ("drone", "P_DR_PRESET"), ("glint", "P_GL_PRESET"))}
        self.words = {k: [t.upper() for t in s["params"][p]["texts"]] for k, p in self.at.items()}
        bass = bytearray(self.size - self.rec)        # an empty bass: every step a CUT of a sixteenth, SEQUENCE off,
        bass[st["bq"] - self.rec + 1] = s["params"][ids["P_BQ_STEPS"]]["def"]   # all its steps, not transposed (app.c
        bass[st["bq"] - self.rec + 2] = s["params"][ids["P_BQ_TRANS"]]["def"]   # bseq_empty), at the default TEMPO
        bass[st["tempo"] - self.rec] = s["params"][ids["P_TEMPO"]]["def"]
        self.empty_bass = bytes(bass)
        if len(self.max) != self.nsound or self.v_at + self.nsound > self.rec or not self.rec <= self.bq < self.size \
                or any(p >= self.nsound for p in self.at.values()):
            raise SystemExit(f"{path}: its scene_t doesn't hold what the River reads (app.c schema_write)")

    # ------------------------------------------------------------------------------------------------ shards
    def name_of(self, data):
        raw = bytes(data[self.name_at:self.name_at + self.name_len])
        return raw.split(b"\0", 1)[0].decode("ascii", "replace")

    def _name(self, name):
        b = name.encode("ascii", "replace") if isinstance(name, str) else bytes(name)
        if not 1 <= len(b) <= self.name_len or not set(b) <= NAME_CHARS or b.strip() == b"":
            raise Invalid(f"A shard's name: 1 to {self.name_len} of A-Z, 0-9, space, - . # +")
        return b.ljust(self.name_len, b"\0")

    def shard(self, data, layout):
        if layout != self.layout:
            raise Invalid(f"This shard was made by another FRASIL ({layout}): the River takes {self.layout} only "
                          "for now. Update FRASIL on the FM-1, then back it up again.")
        if len(data) != self.size:
            raise Invalid(f"A {self.layout} shard is {self.size} bytes, not {len(data)}.")
        for p in range(self.nsound):
            if data[self.v_at + p] > self.max[p]:
                raise Invalid(f"A setting is out of range ({self.s['params'][p]['name']}).")
        b = bytearray(data)                           # (after the name's NUL, the FM-1 may leave an older name's
        b[self.name_at:self.name_at + self.name_len] = self._name(self.name_of(data))   # letters: dropped)
        return bytes(b)

    def renamed(self, data, name):
        b = bytearray(data)
        b[self.name_at:self.name_at + self.name_len] = self._name(name)
        return bytes(b)

    def upgraded(self, data, layout):
        """an earlier shard (the schema's old_shards: its name and nv sounds, the rest of its record as this one's,
        then its bass's sequence or none) -> this layout's bytes: the settings the sound has gained since, at their
        defaults, after it (FRSh, FRSi); its bass's sequence over an empty one (FRSh: none; FRSi, FRSk, FRSm: without
        TEMPO, at its default); GLINT's RELEASE where SCATTER was, as its DECAY let a glint go then (the schema's
        scatter_gone: FRSk and before). A scene
        of an earlier scenes' record that only differs from FRSh's by its name's length (FRSg: names of ten) -> FRSh
        (the name NUL-padded), then on. None if it differs more"""
        n, old = self.s["sizes"], next((x for x in self.s["old_shards"] if x["magic"] == layout), None)
        if old:
            if len(data) != old["size"] or self.name_at != 0 or self.v_at != self.name_len:
                return None
            sv = self.v_at + old["nv"]
            b = bytes(data[:sv]) + bytes(p["def"] for p in self.s["params"][old["nv"]:self.nsound]) + bytes(data[sv:])
            b = bytearray(b + self.empty_bass[len(b) - self.rec:])   # (the rest of an empty bass: all of it, or TEMPO)
            if old.get("scatter"):
                b[self.v_at + self.gone["p"]] = self.gone["of"][b[self.v_at + self.gone["decay"]]]
            b = bytes(b)
            try:
                return self.shard(b, self.layout)
            except Invalid:
                return None
        sc = next((x for x in self.s["old_scenes"] if x["magic"] == layout), None)
        rec = next((x for x in self.s["old_shards"] if x["size"] == x["rec"]), None)   # (FRSh: the record alone)
        if not sc or not rec or sc["nv"] != rec["nv"] or self.name_at != 0 or self.v_at != self.name_len:
            return None
        nl = sc["name"]
        size = nl + sc["nv"] + 2 + n["SC_CHORDS"] * 5 + n["SC_ARP"] + (1 if sc["extra"] else 0) + \
            (n["SC_SEQ"] if sc["extra"] >= 2 else 0)
        if nl >= self.name_len or size - nl != rec["rec"] - self.name_len or len(data) != size:
            return None
        return self.upgraded(bytes(data[:nl]).ljust(self.name_len, b"\0") + bytes(data[nl:]), rec["magic"])

    def derived(self, data):
        return {**{k: data[self.v_at + p] for k, p in self.at.items()}, "bseq": int(data[self.bq] != 0)}

    def value(self, what, text):
        """a filter's value, a word (D#, MINOR, ARP) or its number -> the number; None: not one"""
        t = str(text).strip().upper()
        if t.isascii() and t.isdigit():
            return int(t) if int(t) < len(self.words[what]) else None
        return self.words[what].index(t) if t in self.words[what] else None

    def pack(self, shards, firmware):
        return json.dumps({"format": "frasil-scenes", "version": 1, "layout": self.layout, "firmware": firmware,
                           "scenes": [{"name": sh["name"], **({"from": sh["from_slot"] + 1} if sh["from_slot"]
                                                              is not None else {}),
                                       "data": base64.b64encode(sh["data"]).decode()} for sh in shards]}, indent=1)

    # ------------------------------------------------------------------------------------------------ backups
    def backup(self, image):
        """fm1_backup.py parse() with FRASIL's kind: its magic and version; every part's CRC-32, the whole file's"""
        want = self.s["backup"]
        if len(image) > BACKUP_MAX:
            raise Invalid(f"A backup is {BACKUP_MAX} bytes at most.")
        if len(image) < HEAD + 4:
            raise Invalid("This file is too short for a FRASIL backup.")
        magic, ver, n, _flags, hcrc = struct.unpack_from("<4sHHII", image)
        if magic != want["magic"].encode():
            raise Invalid("This file is not a FRASIL backup.")
        if hcrc != zlib.crc32(image[:12]):
            raise Invalid("This backup is damaged (its head).")
        if ver != want["version"]:
            raise Invalid(f"This is a backup from another version of FRASIL (version {ver}).")
        objs, pos = {}, HEAD
        for _ in range(n):
            if pos + REC > len(image):
                raise Invalid("This backup is cut short.")
            obj, ln, crc = struct.unpack_from("<HHI", image, pos)
            pos += REC
            if ln > PAYLOAD_MAX or pos + ln + 4 > len(image):
                raise Invalid("This backup is damaged (a part's header).")
            if zlib.crc32(image[pos:pos + ln]) != crc:
                raise Invalid("This backup is damaged (a part's CRC-32).")
            objs[obj] = image[pos:pos + ln]
            pos += ln
        if pos + 4 != len(image) or struct.unpack_from("<I", image, pos)[0] != zlib.crc32(image[:pos]):
            raise Invalid("This backup is damaged (the whole file's CRC-32).")
        sc, ss = objs.get(self.s["objs"]["scenes"], b""), self.s["scenes_t"]
        if not sc:                                    # (the scenes' record: their names in it; their basses apart)
            return []
        if sc[:4] != self.s["magic"]["scenes"].encode() or len(sc) != ss["rec"]:
            return None
        used = sc[ss["used"]] | sc[ss["used"] + 1] << 8
        return [self.name_of(sc[ss["s"] + i * self.rec:]) if used >> i & 1 else None
                for i in range(self.s["sizes"]["NSCENES"])]
