#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""The veille's sending (run.sh, on the M3): candidates.json's `auto`, the candidates the agent kept (decisions.json: [{i, summary,
sentiment, priority}], plus whole items it found itself under "extra"), then the pass's end. A decision that doesn't fit is left out, said.
    ingest.py [--failed WHY]        --failed: what is ready only, the pass marked in error"""
import json
import os
import sys
import urllib.error
import urllib.request

WORK = os.path.expanduser("~/.local/state/frasil-veille")
API = "https://admin.frasilsynth.com/api/veille"
TOKEN = open(os.path.expanduser("~/.config/frasil/veille.token")).read().strip()


def post(path, body):
    r = urllib.request.Request(f"{API}/{path}", data=json.dumps(body).encode(), headers={
        "Authorization": "Bearer " + TOKEN, "Content-Type": "application/json", "User-Agent": "frasil-veille/1.0"})
    try:
        return json.load(urllib.request.urlopen(r, timeout=60))
    except urllib.error.HTTPError as e:
        sys.exit(f"{path}: {e.code} {e.read().decode()[:300]}")


def main():
    c = json.load(open(os.path.join(WORK, "candidates.json")))
    failed = sys.argv[2] if sys.argv[1:2] == ["--failed"] else None
    items, notes = list(c["auto"]), list(c["notes"])
    if failed:
        notes.insert(0, failed)
    else:
        d = json.load(open(os.path.join(WORK, "decisions.json")))
        by_i = {x["i"]: x for x in c["candidates"]}
        for k in d["keep"]:
            if k.get("i") not in by_i:
                notes.append(f"décision ignorée : {k.get('i')}")
                continue
            x = {a: b for a, b in by_i[k["i"]].items() if a not in ("i", "heard", "already")}
            items.append({**x, "summary": k["summary"], "sentiment": k["sentiment"], "priority": k["priority"]})
        items += d.get("extra", [])
        notes += d.get("notes", [])
    r = post("ingest", {"run": {"started_at": c["started"]}, "items": items})
    status = "error" if failed else "partial" if notes and any("limit" in n or "Reddit" in n or "Error" in n for n in notes) else "ok"
    post(f"runs/{r['run_id']}/finish", {"status": status, "notes": " · ".join(notes)[:1900]})
    open(os.path.join(WORK, "sent"), "w").close()
    print(r, status)


if __name__ == "__main__":
    main()
