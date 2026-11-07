#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# The names of 32 (3.0.1): names.txt, then from its backup (long.frasil) a scene moved as a River moves it and the copy
# tools/backup_for_3_0.py makes for FRASIL 3.0, both restored (names_moved.txt). FRASIL_30_HOST: an earlier FRASIL's
# simulator (3.0, 3.0.1 before 32-character names): it refuses long.frasil (its sixth object), restores the 3.0 copy, and
# backs it up again object for object as it was given (OBJ_NAMES aside, every record as 3.0 keeps it).
#   app/tests/names.sh [OUTDIR]          (after ./build.sh --host)
set -e
cd "$(dirname "$0")/../.."
H=app/build/host/frasil_host
OUT="${1:-$(mktemp -d)}"
PY="${PYTHON:-python3}"
mkdir -p "$OUT"
$H app/tests/names.txt "$OUT" > "$OUT/names.log" || { grep FAIL "$OUT/names.log"; echo "names: checks failed"; exit 1; }
"$PY" - "$OUT/long.frasil" "$OUT/moved.frasil" <<'EOF'
import struct, sys, zlib
sys.path.insert(0, "tools")
from backup_for_3_0 import objects, build
version, flags, objs = objects(open(sys.argv[1], "rb").read())
o = dict(objs)
SC, BS, DS, FROM, TO = 229, 95, 234, 12, 15          # a scene's in OBJ_SCENES, OBJ_BSEQ, OBJ_DSEQ (FRDR); slot 13 -> slot 16
sc, bs, ds = bytearray(o[2]), bytearray(o[3]), bytearray(o[4])
assert (len(sc), len(bs), len(ds)) == (8 + 16 * SC, 4 + 16 * BS, 4 + 16 * DS), "the layouts names.sh knows"
sc[8 + TO * SC:8 + (TO + 1) * SC] = sc[8 + FROM * SC:8 + (FROM + 1) * SC]
bs[4 + TO * BS:4 + (TO + 1) * BS] = bs[4 + FROM * BS:4 + (FROM + 1) * BS]
ds[4 + TO * DS:4 + (TO + 1) * DS] = ds[4 + FROM * DS:4 + (FROM + 1) * DS]
used = (struct.unpack_from("<H", sc, 4)[0] | 1 << TO) & ~(1 << FROM)
struct.pack_into("<H", sc, 4, used)
o[2], o[3], o[4] = bytes(sc), bytes(bs), bytes(ds)
open(sys.argv[2], "wb").write(build(version, flags, [(k, o[k]) for k, _ in objs]))
EOF
"$PY" tools/backup_for_3_0.py "$OUT/long.frasil" "$OUT/for30.frasil" > /dev/null
$H app/tests/names_moved.txt "$OUT" > "$OUT/moved.log" || { grep FAIL "$OUT/moved.log"; echo "names: moved, for 3.0 failed"; exit 1; }
n=$(( $(grep -c '^  ok' "$OUT/names.log") + $(grep -c '^  ok' "$OUT/moved.log") ))
if [ -n "$FRASIL_30_HOST" ]; then
    mkdir -p "$OUT/30"
    cp "$OUT/long.frasil" "$OUT/for30.frasil" "$OUT/30/"
    printf 'setp 214 1\nsetp 216 0\nsetp 217 0\nsetp 224 0\nwait 600\nrestore out:long.frasil\nwait 50\nexpect restore_rc 3\ntap SEL\nwait 30\nrestore out:for30.frasil\nwait 50\nexpect restore_rc 0\ntap SEL\nwait 30\nprefs\nbackup out:again.frasil\n' > "$OUT/30/down.txt"
    "$FRASIL_30_HOST" "$OUT/30/down.txt" "$OUT/30" > "$OUT/30/down.log" || { grep FAIL "$OUT/30/down.log"; echo "names: 3.0 failed"; exit 1; }
    "$PY" - "$OUT/30/for30.frasil" "$OUT/30/again.frasil" <<'EOF'
import sys
sys.path.insert(0, "tools")
from backup_for_3_0 import objects
a, b = (dict(objects(open(p, "rb").read())[2]) for p in sys.argv[1:3])
assert sorted(a) == sorted(b) == [0, 2, 3, 4, 5], (sorted(a), sorted(b))
for k in (2, 3, 4, 5):
    assert a[k] == b[k], f"object {k} changed"
print("names: 3.0 refused the 3.0.1 backup, restored its 3.0 copy and backed it up again: the scenes the same, byte for byte")
EOF
    n=$(( n + $(grep -c '^  ok' "$OUT/30/down.log") ))
fi
echo "names: ok ($n checks)"
