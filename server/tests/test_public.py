# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""The public River: the list (new, played, favorites, popular), its filters (the bass's sequence too) and search (the
name, the description), its pages; the counters; save (v1's copy); report; the admins' moderation."""
import base64
from collections import deque
import time

from conftest import ADMIN, db, other, signup
from test_shards import IDS, ST, add, b64

V = ST["v"]


def shared(c, data, **kw):
    sid = add(c, data).json()["id"]
    r = c.post(f"/api/shards/{sid}/share", json=kw)
    assert r.status_code == 200, r.text
    return sid


def tuned(sc, root=None, scale=None, type=None):
    b = bytearray(sc)
    for p, x in ((IDS["P_ROOT"], root), (IDS["P_SCALE"], scale), (IDS["P_SQ_TYPE"], type)):
        if x is not None:
            b[V + p] = x
    return bytes(b)


def test_list_pages_and_new_first(client, make, scenes):
    signup(client, verified=True)
    ids = [shared(client, scenes[i % 4], title=f"Number {i}") for i in range(30)]
    d = db(client)
    for i, sid in enumerate(ids):                 # (one a minute)
        d.execute("UPDATE shards SET shared_at = ? WHERE id = ?", (1_800_000_000 + 60 * i, sid))
    anon = make(app=client.app)
    p1, p2 = anon.get("/api/public/shards").json(), anon.get("/api/public/shards", params={"page": 2}).json()
    assert (len(p1["items"]), p1["page"], p1["pages"], p1["total"]) == (24, 1, 2, 30)
    assert [x["id"] for x in p1["items"] + p2["items"]] == ids[::-1]
    x = p1["items"][0]
    assert x["author"] == "Ana" and x["license"] == "CC BY 4.0" and len(base64.b64decode(x["data"])) == ST["size"]
    assert {"id", "name", "description", "author", "data", "created_at", "shared_at", "plays", "sends", "saves",
            "favorites", "favorited", "bseq", "license"} <= set(x)
    assert not {"visibility", "from_slot", "title", "tags"} & set(x)
    assert anon.get("/api/public/shards", params={"page": 3}).json()["items"] == []
    assert anon.get("/api/public/shards", params={"sort": "best"}).status_code == 422


def test_filters_and_search(client, scenes):
    signup(client, verified=True)
    a = shared(client, tuned(scenes[0], root=2, scale=4, type=1), description="Phrygian fog over the lake.",
               title="Mist", tags=["mist"])                                                          # D PHRYGIAN ARP
    b = shared(client, tuned(scenes[1], root=9, scale=2, type=2), description="Night")                # A MINOR SEQ
    c = shared(client, tuned(scenes[2], root=2, scale=2, type=0), description="Une forêt\nsous la neige")   # D MINOR
    e = shared(client, tuned(scenes[3], root=9, scale=2, type=2))             # A MINOR SEQ, its bass's sequence on

    def ids(**q):
        return sorted(x["id"] for x in client.get("/api/public/shards", params=q).json()["items"])

    assert ids(root="D") == sorted([a, c]) and ids(root="2") == sorted([a, c]) and ids(root="a") == sorted([b, e])
    assert ids(scale="minor") == sorted([b, c, e]) and ids(root="D", scale="MINOR") == [c]
    assert ids(type="ARP") == [a] and ids(type="seq") == sorted([b, e]) and ids(type="off") == [c]
    assert ids(bass="seq") == [e] and ids(bass="NONE") == sorted([a, b, c]) and ids(bass="seq", root="D") == []
    assert [x["bseq"] for x in client.get("/api/public/shards", params={"bass": "seq"}).json()["items"]] == [1]
    assert ids(q="fog") == [a] and ids(q="NIGHT") == [b] and ids(q="phrygian lake") == [a] and ids(q="fog night") == []
    assert ids(q="CATHEDRAL") == [b] and ids(q="100%") == [] and ids(q="forêt") == [c] and ids(q="neige") == [c]
    assert ids(q="mist") == [] and ids(q="aurora") == [e]                    # (a title, tags: ignored)
    for bad in ({"root": "H"}, {"scale": "99"}, {"type": "loop"}, {"bass": "on"}):
        assert client.get("/api/public/shards", params=bad).status_code == 422


def test_sorts(client, make, scenes):
    """new (the default: the newest shared first), played (the most played), favorites (the most favorited), popular"""
    signup(client, verified=True)
    a, b, c = (shared(client, sc) for sc in scenes[:3])
    d, t = db(client), int(time.time())
    for sid, ago, plays, sends in ((a, 200, 15, 0), (b, 300, 2, 5), (c, 100, 0, 0)):
        d.execute("UPDATE shards SET shared_at = ?, plays = ?, sends = ? WHERE id = ?", (t - ago, plays, sends, sid))
    for i, sids in enumerate(((c, b), (c,))):
        f = other(client, make, email=f"f{i}@river.test", name=f"F{i}")
        for sid in sids:
            assert f.post(f"/api/favorites/{sid}", json={}).status_code == 201

    def order(sort=None):
        j = client.get("/api/public/shards", params={"sort": sort} if sort else {}).json()
        return j["sort"], [x["id"] for x in j["items"]]

    assert order() == ("new", [c, a, b]) and order("new") == order()
    assert order("played") == ("played", [a, b, c])
    assert order("favorites") == ("favorites", [c, b, a])
    assert order("popular") == ("popular", [b, a, c])         # (b: 1 + 2 + 3 x 5 + 5 x 1; a: 1 + 15; c: 1 + 5 x 2)
    assert [x["favorites"] for x in client.get("/api/public/shards", params={"sort": "favorites"}).json()["items"]
            ] == [2, 1, 0]


def test_popular(client, make, scenes):
    signup(client, verified=True)
    old, new, quiet = (shared(client, sc) for sc in scenes[:3])
    d = db(client)
    week = int(time.time()) - 7 * 86400
    d.execute("UPDATE shards SET shared_at = ?, saves = 30 WHERE id = ?", (week, old))
    d.execute("UPDATE shards SET plays = 3, sends = 1 WHERE id = ?", (new,))
    items = client.get("/api/public/shards", params={"sort": "popular"}).json()["items"]
    assert [x["id"] for x in items] == [old, new, quiet]
    d.execute("UPDATE shards SET shared_at = ? WHERE id = ?", (week - 120 * 86400, old))   # (fading)
    items = client.get("/api/public/shards", params={"sort": "popular"}).json()["items"]
    assert [x["id"] for x in items] == [new, quiet, old]


def test_counters_once_a_day_per_listener(client, make, scenes):
    signup(client, verified=True)
    sid = shared(client, scenes[0])
    anon = make(app=client.app)
    ip = {"CF-Connecting-IP": "198.51.100.7"}
    assert anon.post(f"/api/public/shards/{sid}/played", json={}, headers=ip).json() == {"plays": 1}
    assert anon.post(f"/api/public/shards/{sid}/played", json={}, headers=ip).json() == {"plays": 1}
    assert anon.post(f"/api/public/shards/{sid}/played", json={},
                     headers={"CF-Connecting-IP": "198.51.100.8"}).json() == {"plays": 2}
    assert client.post(f"/api/public/shards/{sid}/played", json={}).json() == {"plays": 2}   # (the author's own)
    assert anon.post(f"/api/public/shards/{sid}/sent", json={}, headers=ip).json() == {"sends": 1}
    b = other(client, make, email="bo@river.test", name="Bo")
    assert b.post(f"/api/public/shards/{sid}/sent", json={}, headers=ip).json() == {"sends": 2}   # (a user, not an IP)
    assert "198.51.100.7" not in str(db(client).execute("SELECT * FROM hits").fetchall())
    assert anon.post("/api/public/shards/nothing/played", json={}).status_code == 404
    anon.app.state.limiter.hits["count:198.51.100.9"] = deque([time.monotonic()] * 300)
    r = anon.post(f"/api/public/shards/{sid}/played", json={}, headers={"CF-Connecting-IP": "198.51.100.9"})
    assert r.status_code == 429 and int(r.headers["retry-after"]) > 0


def test_save_copies_and_credits(client, make, scenes):
    signup(client, verified=True, name="Ana")
    sid = shared(client, scenes[0], description="Abyss, deep.", title="Abyss", tags=["deep"])
    anon = make(app=client.app)
    assert anon.post(f"/api/public/shards/{sid}/save", json={}).status_code == 401
    b = other(client, make, email="bo@river.test", name="Bo", verified=True)
    r = b.post(f"/api/public/shards/{sid}/save", json={})
    copy = r.json()
    assert r.status_code == 201 and copy["origin_id"] == sid and copy["origin_author"] == "Ana"
    assert copy["visibility"] == "private" and copy["data"] == b64(scenes[0])
    assert (copy["description"], copy["saves"], copy["bseq"]) == ("Abyss, deep.", 0, 0) and "title" not in copy
    again = b.post(f"/api/public/shards/{sid}/save", json={})
    assert again.status_code == 200 and again.json()["id"] == copy["id"]
    assert client.get(f"/api/public/shards/{sid}").json()["saves"] == 1
    b.delete(f"/api/shards/{copy['id']}")
    assert b.post(f"/api/public/shards/{sid}/save", json={}).status_code == 201
    assert client.get(f"/api/public/shards/{sid}").json()["saves"] == 1          # (each user once, ever)
    copy = b.get("/api/shards").json()["items"][0]
    assert client.post(f"/api/public/shards/{sid}/save", json={}).status_code == 409   # (hers already)
    client.patch("/api/me", json={"display_name": "Ana Lune"})
    assert b.get("/api/shards").json()["items"][0]["origin_author"] == "Ana Lune"
    b.post(f"/api/shards/{copy['id']}/share", json={})                       # Bo shares his copy; Cy saves it:
    c = other(client, make, email="cy@river.test", name="Cy")
    third = c.post(f"/api/public/shards/{copy['id']}/save", json={}).json()
    assert third["origin_id"] == copy["id"] and third["origin_author"] == "Ana Lune"   # (still its first author)
    pub = client.get(f"/api/public/shards/{copy['id']}").json()
    assert pub["author"] == "Bo" and pub["origin_author"] == "Ana Lune"


def test_report_and_moderation(client, make, scenes):
    signup(client, verified=True)
    sid = shared(client, scenes[0])
    keep = shared(client, scenes[1])
    anon = make(app=client.app)
    assert anon.post(f"/api/public/shards/{sid}/report", json={"reason": " "}).status_code == 422
    for _ in range(2):                            # (once per reporter)
        assert anon.post(f"/api/public/shards/{sid}/report", json={"reason": "Spam"}).status_code == 201
    assert client.get("/api/admin/reports").status_code == 403
    a = other(client, make, email=ADMIN, name="Admin", verified=True)
    (rep,) = a.get("/api/admin/reports").json()["items"]
    assert rep["reason"] == "Spam" and rep["shard"]["id"] == sid and rep["by_user"] is None
    assert a.post(f"/api/admin/shards/{sid}/hide", json={}).json()["visibility"] == "hidden"
    assert anon.get(f"/api/public/shards/{sid}").status_code == 404
    assert a.get("/api/admin/reports").json()["items"] == []
    assert len(a.get("/api/admin/reports", params={"all": True}).json()["items"]) == 1
    assert [x["id"] for x in client.get("/api/shards").json()["items"] if x["visibility"] == "hidden"] == [sid]
    assert a.post(f"/api/admin/shards/{sid}/unhide", json={}).json()["visibility"] == "public"
    assert anon.get(f"/api/public/shards/{sid}").status_code == 200
    assert a.post(f"/api/admin/shards/{sid}/delete", json={}).json() == {"ok": True}
    assert anon.get(f"/api/public/shards/{sid}").status_code == 404
    assert len(client.get("/api/shards").json()["items"]) == 1
    assert a.post("/api/admin/shards/nothing/hide", json={}).status_code == 404
    uid = client.get("/api/me").json()["id"]
    a.post(f"/api/admin/users/{uid}/ban", json={})
    assert anon.get(f"/api/public/shards/{keep}").status_code == 404           # (a banned user's: out of the River)
    assert anon.get("/api/public/shards").json()["total"] == 0
    a.post(f"/api/admin/users/{uid}/unban", json={})
    assert anon.get("/api/public/shards").json()["total"] == 1
