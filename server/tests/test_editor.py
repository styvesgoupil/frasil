# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""The River's editor: SAVE (one's own shard's sound replaced: checked as a new one's, its name kept, what the lists
filter by read again) and SAVE AS (a new shard made from another: someone else's public one credited, CC BY 4.0, one's
own passing its credit on, or none)."""
import base64

from conftest import db, other, signup
from test_public import shared, tuned
from test_shards import IDS, LAYOUT, NL, S, ST, add, b64

V = ST["v"]
DECAY = 21                                            # (CLOUD's DECAY: a sound setting, its range 0 .. max)


def turned(sc, p=DECAY, x=None):
    b = bytearray(sc)
    b[V + p] = x if x is not None else (b[V + p] + 9) % (S["params"][p]["max"] + 1)
    return bytes(b)


def test_save_replaces_the_sound(client, make, scenes):
    signup(client, verified=True, name="Ana")
    j = add(client, scenes[0], name="deep").json()
    new = turned(tuned(base64.b64decode(j["data"]), root=5))
    r = client.patch(f"/api/shards/{j['id']}", json={"data": b64(new), "layout": LAYOUT, "firmware": "2.0.1"})
    e = r.json()
    assert r.status_code == 200 and base64.b64decode(e["data"]) == new and e["name"] == "DEEP"
    assert e["root"] == 5 and e["firmware"] == "2.0.1" and e["description"] == j["description"]
    other_name = b"ELSEWHERE".ljust(NL, b"\0") + turned(new)[NL:]     # (its name stays the shard's)
    e = client.patch(f"/api/shards/{j['id']}", json={"data": b64(other_name), "layout": LAYOUT}).json()
    assert e["name"] == "DEEP" and base64.b64decode(e["data"])[:NL] == b"DEEP".ljust(NL, b"\0") and e["firmware"] == "2.0.1"
    e = client.patch(f"/api/shards/{j['id']}", json={"data": b64(new), "layout": LAYOUT, "name": "deeper"}).json()
    assert e["name"] == "DEEPER" and base64.b64decode(e["data"])[NL:] == new[NL:]
    for bad in ({"data": b64(new), "layout": "FRSa"}, {"data": b64(turned(new, x=255)), "layout": LAYOUT},
                {"data": b64(new[:-3]), "layout": LAYOUT}, {"data": "%%%", "layout": LAYOUT},
                {"data": b64(new), "layout": LAYOUT, "firmware": "2.0 beta!"}):
        assert client.patch(f"/api/shards/{j['id']}", json=bad).status_code == 422, bad
    b = other(client, make, email="bo@river.test", name="Bo")
    assert b.patch(f"/api/shards/{j['id']}", json={"data": b64(new), "layout": LAYOUT}).status_code == 404


def test_save_as_credits_the_first_author(client, make, scenes):
    signup(client, verified=True, name="Ana")
    sid = shared(client, scenes[0], description="Abyss, deep.")
    private = add(client, scenes[1]).json()["id"]
    b = other(client, make, email="bo@river.test", name="Bo", verified=True)
    made = add(b, turned(scenes[0]), name="abyss ii", origin=sid).json()     # someone else's: credited
    assert made["name"] == "ABYSS II" and made["origin_id"] == sid and made["origin_author"] == "Ana"
    again = add(b, turned(scenes[0], x=3), name="abyss iii", origin=made["id"]).json()   # his own, credited: passed on
    assert again["origin_id"] == sid and again["origin_author"] == "Ana"
    plain = add(b, scenes[2]).json()
    mine = add(b, turned(scenes[2]), origin=plain["id"]).json()                # his own, uncredited: none
    assert mine["origin_id"] is None and mine["origin_author"] is None
    assert add(b, scenes[0], origin=private).status_code == 404                 # (not in the River)
    assert add(b, scenes[0], origin="nope").status_code == 404
    own = add(client, turned(scenes[0]), origin=sid).json()                     # Ana from her own shared one: none
    assert own["origin_id"] is None and own["origin_author"] is None
    b.post(f"/api/shards/{made['id']}/share", json={})
    pub = client.get(f"/api/public/shards/{made['id']}").json()
    assert pub["author"] == "Bo" and pub["origin_author"] == "Ana"


def test_save_as_from_a_favorite_no_longer_shared_and_not_from_a_hidden_one(client, make, scenes):
    signup(client, verified=True, name="Ana")
    sid = shared(client, scenes[0])
    gone = shared(client, scenes[1])
    b = other(client, make, email="bo@river.test", name="Bo", verified=True)
    assert b.post(f"/api/favorites/{sid}", json={}).status_code in (200, 201)
    client.post(f"/api/shards/{sid}/unshare", json={})                        # (still among Bo's favorites)
    made = add(b, turned(scenes[0]), origin=sid).json()
    assert made["origin_id"] == sid and made["origin_author"] == "Ana"
    con = db(client)
    con.execute("UPDATE shards SET visibility = 'hidden' WHERE id = ?", (gone,))   # (the moderators hid it)
    con.commit()
    assert add(b, scenes[1], origin=gone).status_code == 404
    c = other(client, make, email="cy@river.test", name="Cy", verified=True)
    b.post(f"/api/shards/{made['id']}/share", json={})                         # Cy from Bo's (Ana's first): Ana
    third = add(c, turned(scenes[0], x=1), origin=made["id"]).json()
    assert third["origin_id"] == made["id"] and third["origin_author"] == "Ana"


def test_save_reads_every_column_again_and_share_takes_no_sound(client, make, scenes):
    signup(client, verified=True, name="Ana")
    j = add(client, scenes[0]).json()
    new = tuned(turned(base64.b64decode(j["data"])), root=2, scale=1, type=2)
    e = client.patch(f"/api/shards/{j['id']}", json={"data": b64(new), "layout": LAYOUT}).json()
    assert (e["root"], e["scale"], e["type"]) == (2, 1, 2)
    row = db(client).execute("SELECT layout, root, scale, type FROM shards WHERE id = ?", (j["id"],)).fetchone()
    assert tuple(row) == (LAYOUT, 2, 1, 2)
    r = client.post(f"/api/shards/{j['id']}/share", json={"data": b64(scenes[1]), "layout": LAYOUT})
    assert r.status_code == 422 and client.get("/api/shards").json()["items"][0]["data"] == e["data"]
