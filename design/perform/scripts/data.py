# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# Perform's data, as scripts the pages load (they run from file:// too):
#   data/perform-schema.js  from the schema (server/schema.json), what the FM-1 would tell the page: the pages, each
#                           setting's name and its value as the FM-1 writes it, the factory scenes (their scene_t bytes,
#                           as the engine takes them)
#   data/perform-data.js    the mockups' river: what the engine played (data/live.json, from
#                           design/river-live/scripts/capture.mjs: web_live's 52 floats, 30 frames a second)
#   .venv/bin/python design/perform/scripts/data.py
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
REPO = HERE.parent.parent
S = json.loads((REPO / "server/schema.json").read_text())
nlen = S["scene_t"]["name_len"]
factory = []
for h in S["factory"][:S["sizes"]["NFACTORY"]]:
    factory.append({"name": bytes.fromhex(h)[:nlen].split(b"\0")[0].decode(), "hex": h})
schema = {
    "params": [{"name": p["name"], "kind": p["kind"], "max": p["max"], "texts": p.get("texts")} for p in S["params"]],
    "pages": [{"name": g["name"], "button": g.get("button"), "p": g["p"]} for g in S["pages"]],
    "factory": factory,
    "v0": S["scene_t"]["v"],
    "nsound": S["sizes"]["NSOUND"],
    "tempo": S["ids"].get("P_TEMPO"),
    "lfo": S["lfo"],
}
(HERE / "data/perform-schema.js").write_text("window.PERFORM_SCHEMA = " + json.dumps(schema, separators=(",", ":")) + ";\n")
live = json.loads((HERE / "data/live.json").read_text())
(HERE / "data/perform-data.js").write_text("window.PERFORM_DATA = " + json.dumps({"live": live}, separators=(",", ":")) + ";\n")
print("ok", [f["name"] for f in factory], "| mockups:", list(live))
