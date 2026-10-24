# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""What a user has: all of it in a zip (the page's own formats); all of it gone with the account."""
import base64
import io
import json
import zipfile

from conftest import db, other, signup
from test_public import shared
from test_shards import LAYOUT, add, b64


def test_export(client, scenes, example):
    signup(client, verified=True)
    a = add(client, scenes[0], from_slot=2).json()
    b = add(client, scenes[1]).json()
    client.post("/api/backups", json={"image": b64(example), "note": "gig"})
    r = client.get("/api/me/export")
    assert r.status_code == 200 and r.headers["content-type"] == "application/zip"
    assert r.headers["content-disposition"].startswith('attachment; filename="river-')
    z = zipfile.ZipFile(io.BytesIO(r.content))
    names = z.namelist()
    (frasil,) = [n for n in names if n.startswith("backups/") and n.endswith(".frasil")]
    assert z.read(frasil) == example
    j = json.loads(z.read("account.json"))
    assert j["account"]["email"] == "ana@river.test" and "$argon2" not in json.dumps(j)
    assert {x["id"] for x in j["shards"]} == {a["id"], b["id"]} and j["backups"][0]["file"] == frasil
    pack = json.loads(z.read(f"shards-{LAYOUT}.frasilscenes"))      # (FB.packOf's: what FB.readPack reads)
    assert (pack["format"], pack["version"], pack["layout"], pack["firmware"]) == ("frasil-scenes", 1, LAYOUT, "1.1")
    by = {s["name"]: s for s in pack["scenes"]}
    assert by["ABYSS"]["from"] == 3 and "from" not in by["CATHEDRAL"]
    assert base64.b64decode(by["ABYSS"]["data"]) == scenes[0]
    client.cookies.clear()
    assert client.get("/api/me/export").status_code == 401


def test_delete_everything(client, make, scenes, example):
    signup(client, verified=True)
    sid = shared(client, scenes[0])
    client.post("/api/backups", json={"image": b64(example)})
    bo = other(client, make, email="bo@river.test", name="Bo")
    copy = bo.post(f"/api/public/shards/{sid}/save", json={}).json()
    bo.post(f"/api/public/shards/{sid}/report", json={"reason": "too quiet"})
    client.post(f"/api/public/shards/{copy['id']}/played", json={})
    me = client.get("/api/me").json()["id"]
    r = client.delete("/api/me")
    assert r.status_code == 200 and client.get("/api/me").status_code == 401
    d = db(client)
    for table, col in (("users", "id"), ("sessions", "user_id"), ("shards", "owner_id"), ("backups", "owner_id"),
                       ("tokens", "user_id")):
        assert d.execute(f"SELECT COUNT(*) FROM {table} WHERE {col} = ?", (me,)).fetchone()[0] == 0, table
    assert d.execute("SELECT COUNT(*) FROM reports").fetchone()[0] == 0
    assert d.execute("SELECT COUNT(*) FROM hits WHERE who = ?", (f"u{me}",)).fetchone()[0] == 0
    (kept,) = bo.get("/api/shards").json()["items"]      # Bo's copy is his: kept, without her name
    assert kept["id"] == copy["id"] and kept["origin_author"] is None
    assert client.post("/api/auth/login", json={"email": "ana@river.test", "password": "drone in the fog"}
                       ).status_code == 401
