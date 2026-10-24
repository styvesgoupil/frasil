# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""Favorites: a bookmark to someone else's shard in the River, not a copy. Once each (again: the same), never one's
own; listed the newest first, with what plays it and sends it to the FM-1; its author unshares it: kept, marked;
deleted: gone; hidden, or its author banned: left out (back when undone). The River's counts and flags; popular's
kept (each user once, whether they saved a copy before or favorite it)."""
import base64
import time

from conftest import ADMIN, PASSWORD, db, other, signup
from test_public import shared
from test_shards import LAYOUT, add


def test_favorite_again_own(client, make, scenes):
    signup(client, verified=True)
    sid = shared(client, scenes[3], description="Dawn over the ice.")
    mine = add(client, scenes[0]).json()["id"]    # (not shared)
    anon = make(app=client.app)
    assert anon.post(f"/api/favorites/{sid}", json={}).status_code == 401
    assert anon.get("/api/favorites").status_code == 401
    b = other(client, make, email="bo@river.test", name="Bo")
    r = b.post(f"/api/favorites/{sid}", json={})
    assert r.status_code == 201 and r.json() == {"favorited": True, "favorites": 1}
    r = b.post(f"/api/favorites/{sid}", json={})  # (again: the same)
    assert r.status_code == 200 and r.json() == {"favorited": True, "favorites": 1}
    r = client.post(f"/api/favorites/{sid}", json={})
    assert r.status_code == 409 and "yours" in r.json()["detail"]
    assert b.post(f"/api/favorites/{mine}", json={}).status_code == 404       # (not in the River)
    assert b.post("/api/favorites/nothing", json={}).status_code == 404
    assert b.get("/api/shards").json()["items"] == []                          # (a bookmark: no copy)
    c = other(client, make, email="cy@river.test", name="Cy")
    assert c.post(f"/api/favorites/{sid}", json={}).json() == {"favorited": True, "favorites": 2}
    assert b.delete(f"/api/favorites/{sid}").json() == {"favorited": False, "favorites": 1}
    assert b.delete(f"/api/favorites/{sid}").json() == {"favorited": False, "favorites": 1}   # (not one: nothing)
    assert b.get("/api/favorites").json()["items"] == []
    assert db(client).execute("SELECT COUNT(*) FROM favorites").fetchone()[0] == 1


def test_counts_and_flags(client, make, scenes):
    """the River's list and one shard: its favorites, and whether it is among mine (false signed out, or the author);
    my own library: its favorites too"""
    signup(client, verified=True)
    sid, other_sid = shared(client, scenes[0]), shared(client, scenes[1])
    b = other(client, make, email="bo@river.test", name="Bo")
    c = other(client, make, email="cy@river.test", name="Cy")
    b.post(f"/api/favorites/{sid}", json={})
    c.post(f"/api/favorites/{sid}", json={})
    anon = make(app=client.app)

    def seen(who):
        one = who.get(f"/api/public/shards/{sid}").json()
        listed = {x["id"]: x for x in who.get("/api/public/shards").json()["items"]}
        assert (listed[sid]["favorites"], listed[sid]["favorited"]) == (one["favorites"], one["favorited"])
        return one["favorites"], one["favorited"], listed[other_sid]["favorites"], listed[other_sid]["favorited"]

    assert seen(b) == (2, True, 0, False)
    assert seen(anon) == (2, False, 0, False) and seen(client) == (2, False, 0, False)
    assert {x["id"]: x["favorites"] for x in client.get("/api/shards").json()["items"]} == {sid: 2, other_sid: 0}
    b.delete(f"/api/favorites/{sid}")
    assert seen(b) == (1, False, 0, False)


def test_my_favorites(client, make, scenes):
    """the newest first, each with what plays it and sends it to the FM-1: its data, layout, name, description,
    author, dates"""
    signup(client, verified=True, name="Ana")
    ids = [shared(client, sc, description=f"Number {i}.") for i, sc in enumerate(scenes)]
    b = other(client, make, email="bo@river.test", name="Bo")
    for sid in ids[:3]:
        b.post(f"/api/favorites/{sid}", json={})
    db(client).execute("UPDATE favorites SET created_at = created_at + 60 WHERE shard_id = ?", (ids[0],))
    items = b.get("/api/favorites").json()["items"]
    assert [x["id"] for x in items] == [ids[0], ids[2], ids[1]]   # (the same second: the last made first)
    x = items[0]
    assert base64.b64decode(x["data"]) == scenes[0] and x["layout"] == LAYOUT and x["name"] == "ABYSS"
    assert (x["description"], x["author"], x["shared"], x["favorited"], x["favorites"], x["bseq"]) == (
        "Number 0.", "Ana", True, True, 1, 0)
    assert x["created_at"] and x["shared_at"] and x["favorited_at"] > x["shared_at"] and x["license"] == "CC BY 4.0"
    assert not {"title", "tags", "visibility", "from_slot"} & set(x)
    assert client.get("/api/favorites").json()["items"] == []


def test_unshared_hidden_deleted(client, make, scenes):
    """unshared: kept, marked (shared: false), its data still there; hidden or its author banned: left out, back when
    undone; deleted (by its author or an admin): gone"""
    signup(client, verified=True, name="Ana")
    gone, quiet, hid = (shared(client, sc) for sc in scenes[:3])
    b = other(client, make, email="bo@river.test", name="Bo")
    for sid in (gone, quiet, hid):
        b.post(f"/api/favorites/{sid}", json={})

    def mine():
        return {x["id"]: x for x in b.get("/api/favorites").json()["items"]}

    client.post(f"/api/shards/{quiet}/unshare", json={})
    x = mine()[quiet]
    assert x["shared"] is False and base64.b64decode(x["data"]) == scenes[1] and x["author"] == "Ana"
    assert b.get(f"/api/public/shards/{quiet}").status_code == 404
    assert b.post(f"/api/favorites/{quiet}", json={}).status_code == 404       # (out of the River: not again)
    client.patch(f"/api/shards/{quiet}", json={"name": "quiet"})               # (a bookmark: its author's as it is)
    assert mine()[quiet]["name"] == "QUIET"
    client.post(f"/api/shards/{quiet}/share", json={})
    assert mine()[quiet]["shared"] is True
    a = other(client, make, email=ADMIN, name="Admin", verified=True)
    a.post(f"/api/admin/shards/{hid}/hide", json={})
    assert set(mine()) == {gone, quiet}
    a.post(f"/api/admin/shards/{hid}/unhide", json={})
    assert set(mine()) == {gone, quiet, hid}
    client.delete(f"/api/shards/{gone}")
    assert set(mine()) == {quiet, hid}
    a.post(f"/api/admin/shards/{hid}/delete", json={})
    assert set(mine()) == {quiet}
    assert db(client).execute("SELECT COUNT(*) FROM favorites").fetchone()[0] == 1
    uid = client.get("/api/me").json()["id"]
    a.post(f"/api/admin/users/{uid}/ban", json={})
    assert mine() == {}
    a.post(f"/api/admin/users/{uid}/unban", json={})
    assert set(mine()) == {quiet}
    client.post("/api/auth/login", json={"email": "ana@river.test", "password": PASSWORD})
    client.post(f"/api/shards/{quiet}/unshare", json={})
    assert b.delete(f"/api/favorites/{quiet}").json() == {"favorited": False, "favorites": 0}   # (unshared: leaves too)
    assert mine() == {}


def test_favorites_go_with_accounts(client, make, scenes):
    signup(client, verified=True)
    sid = shared(client, scenes[0])
    b = other(client, make, email="bo@river.test", name="Bo", verified=True)
    bsid = shared(b, scenes[1])
    b.post(f"/api/favorites/{sid}", json={})
    client.post(f"/api/favorites/{bsid}", json={})
    assert b.delete("/api/me").status_code == 200                              # (his, and those of his shards)
    assert db(client).execute("SELECT COUNT(*) FROM favorites").fetchone()[0] == 0
    assert client.get(f"/api/public/shards/{sid}").json()["favorites"] == 0
    assert client.get("/api/favorites").json()["items"] == []


def test_popular_counts_who_kept_it_once(client, make, scenes):
    """store.popular's kept: the saves (v1's copies), then the favorites of those who hadn't saved it"""
    signup(client, verified=True)
    x, y = shared(client, scenes[0]), shared(client, scenes[1])
    d, t = db(client), int(time.time())
    d.execute("UPDATE shards SET shared_at = ? WHERE id = ?", (t - 100, x))   # (x the newer: first on a tie)
    d.execute("UPDATE shards SET shared_at = ? WHERE id = ?", (t - 200, y))
    bo, cy, di = (other(client, make, email=f"{n}@river.test", name=n) for n in ("bo", "cy", "di"))
    assert bo.post(f"/api/public/shards/{x}/save", json={}).status_code == 201
    bo.post(f"/api/favorites/{x}", json={})       # (Bo copied it, then favorites it: once)
    cy.post(f"/api/favorites/{y}", json={})
    di.post(f"/api/favorites/{y}", json={})

    def popular():
        return [s["id"] for s in client.get("/api/public/shards", params={"sort": "popular"}).json()["items"]]

    assert popular() == [y, x]
    di.post(f"/api/public/shards/{x}/save", json={})   # (two each now: x the newer)
    assert popular() == [x, y]
