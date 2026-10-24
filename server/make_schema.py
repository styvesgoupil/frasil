#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""The River's schema, for the service: what the firmware's own code writes (the simulator's "schema FILE": app.c
schema_write, as website/assemble.py has it for the page) and the backups' magic and version (app.h
APP_BACKUP_MAGIC, APP_BACKUP_VERSION, read as fm1-base's make_site.py reads them). Nothing in it is copied by hand.
Run it after ./build.sh --host, before the tests and before deploy.sh (which runs it):

    python3 server/make_schema.py [OUT]       (default server/schema.json)
"""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
HOST = REPO / "app" / "build" / "host" / "frasil_host"


def main(out=HERE / "schema.json"):
    if not HOST.exists():
        sys.exit("make_schema: no simulator in app/build/host: run ./build.sh --host first")
    sys.path.insert(0, str(REPO / "base" / "tools"))
    from make_site import backup_values
    with tempfile.TemporaryDirectory() as work:
        script = Path(work) / "schema.txt"
        script.write_text(f"schema {Path(work) / 'schema.json'}\n", encoding="utf-8")
        subprocess.run([str(HOST), str(script), work], check=True, capture_output=True)
        s = json.loads((Path(work) / "schema.json").read_text(encoding="utf-8"))
    v = backup_values(REPO / "app", {})
    if s.get("format") != "frasil-schema" or v["MAGIC"] != s["magic"]["backup"]:
        sys.exit("make_schema: the simulator's schema and app.h disagree (app.c schema_write, app.h APP_BACKUP_MAGIC)")
    s["backup"] = {"magic": v["MAGIC"], "version": int(v["BACKUP_VERSION"])}
    s["firmware"] = (REPO / "VERSION").read_text(encoding="utf-8").strip()
    Path(out).write_text(json.dumps(s, separators=(",", ":")), encoding="utf-8")
    print(f"make_schema: {out} (scenes {s['magic']['scenes']}, scene_t {s['scene_t']['size']} bytes, "
          f"FRASIL {s['firmware']})")


if __name__ == "__main__":
    main(*sys.argv[1:2])
