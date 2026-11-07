#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""ENERGY's calibration on the catalogue (the admin plan's §7): the River's public shards (a copy of its SQLite, opened
read-only; only the shards' names and their artists' display names are read), the test programme's takes, the factory
scenes. r0 the median raw, r98 its 98th percentile (a piece the same as one before, as a River shard that is a factory
scene untouched, counted once), written to energy-calibration.json; every piece measured, as JSON lines.

    engine/tools/energy_catalogue.py [--db river.sqlite] [--len 180000] [--w G,B,D,H] [--write] [--json OUT.jsonl]
      --write  energy-calibration.json rewritten (else only shown)"""
import argparse
import json
import sqlite3
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from energy import CALIBRATION, measure  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
TEST = REPO / "website" / "stream" / "test-programme"
TAKES = {"test-take-1.frstream": "NORTH RIVER", "test-take-2.frstream": "LONG NIGHT", "test-take-3.frstream": "THAW"}


def pct(xs, p):
    xs = sorted(xs)
    k = (len(xs) - 1) * p
    i = int(k)
    return xs[i] + (xs[min(i + 1, len(xs) - 1)] - xs[i]) * (k - i)


def pieces(db, length):
    if db:
        con = sqlite3.connect(f"file:{db}?mode=ro", uri=True)
        rows = con.execute("SELECT s.id, s.name, u.display_name, s.data FROM shards s JOIN users u ON u.id = s.owner_id "
                           "WHERE s.visibility = 'public' AND u.banned_at IS NULL ORDER BY s.shared_at").fetchall()
        for sid, name, by, data in rows:
            yield {"src": "river", "id": sid, "name": name, "by": by}, measure(data, length_ms=length, **W)
    for f, name in TAKES.items():
        yield {"src": "test", "id": f, "name": name, "by": ""}, measure((TEST / f).read_bytes(), **W)
    for i in range(64):
        try:
            m = measure_factory(i, length)
        except Exception:
            break
        yield {"src": "factory", "id": str(i), "name": m["name"], "by": "FRASIL"}, m


def measure_factory(i, length):
    import subprocess
    from energy import tool
    args = [tool(), "--factory", str(i), "--len", str(length)]
    if W:
        args += ["--w", ",".join(map(repr, W["weights"]))]
    r = subprocess.run(args, capture_output=True, check=True)
    return json.loads(r.stdout)


W = {}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--db")
    ap.add_argument("--len", type=int, default=180000)
    ap.add_argument("--w")
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--json")
    a = ap.parse_args()
    if a.w:
        W["weights"] = [float(x) for x in a.w.split(",")]
    rows, seen = [], {}
    for p, m in pieces(a.db, a.len):   # (a River shard that is a factory scene as it came, measured once for the curve)
        key = (m["kind"], m["G"], m["B"], m["D"], m["H"], m["bpm"])
        p["same_as"] = seen.get(key)
        seen.setdefault(key, f'{p["src"]}:{p["id"]}')
        rows.append((p, m))
    raws = [m["raw"] for p, m in rows if not p["same_as"]]
    r0, r98 = round(statistics.median(raws), 2), round(pct(raws, 0.98), 2)
    cal = {"r0": r0, "r98": r98, "weights": W.get("weights", [1, 0.8, 0.5, 4]), "pieces": len(raws),
           "made": "engine/tools/energy_catalogue.py: the River's public shards, the test takes, the factory scenes"}
    out = sys.stdout if not a.json else open(a.json, "w")
    for p, m in rows:
        out.write(json.dumps({**p, **m}) + "\n")
    print(json.dumps(cal), file=sys.stderr)
    if a.write:
        CALIBRATION.write_text(json.dumps(cal, indent=2) + "\n")


if __name__ == "__main__":
    main()
