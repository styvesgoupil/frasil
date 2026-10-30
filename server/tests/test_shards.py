# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""My library: shards (each validated with the simulator's schema; the bass's sequence kept, bseq read from it), their
edits (a name and a description: titles and tags ignored), sharing; my backups (from before the bass's sequence too:
no OBJ_BSEQ); at startup, the shards of an earlier layout made over (FRSk, FRSi, FRSh, FRSg: chained), titles and
tags folded away, bseq filled."""
import base64
import json

from conftest import PASSWORD, SERVER, db, image, link, other, signup

S = json.loads((SERVER / "schema.json").read_text())
ST, IDS, LAYOUT = S["scene_t"], S["ids"], S["magic"]["shard"]
NL, BN = ST["name_len"], ST["bnote"]                # (the bass's sequence: from bnote on, after GLINT's)
EMPTY = bytearray(ST["size"] - BN)                    # an empty bass (app.c bseq_empty)
EMPTY[ST["bq"] - BN + 1], EMPTY[ST["bq"] - BN + 2] = (S["params"][IDS[p]]["def"] for p in ("P_BQ_STEPS", "P_BQ_TRANS"))
TEMPO_DEF = S["params"][IDS["P_TEMPO"]]["def"]
EMPTY[ST["tempo"] - BN] = TEMPO_DEF                  # (at the default TEMPO)
EMPTY = bytes(EMPTY)
V, NS, OLD = ST["v"], S["sizes"]["NSOUND"], {x["magic"]: x for x in S["old_shards"]}
ONV = OLD["FRSi"]["nv"]                               # (the sound before DECAY, SUSTAIN and PULSE)
DEFS = bytes(p["def"] for p in S["params"][ONV:NS])
GONE = S["scatter_gone"]                              # (GLINT's RELEASE where every earlier layout kept SCATTER)


def gone(sc):
    """RELEASE as the scene's DECAY let a glint go before it (where SCATTER was)"""
    b = bytearray(sc)
    b[V + GONE["p"]] = GONE["of"][b[V + GONE["decay"]]]
    return bytes(b)


def frsi(sc):
    """a scene_t as FRSi kept it: without the settings the sound has gained since, nor its TEMPO"""
    return sc[:V + ONV] + sc[V + NS:-1]


def frsh(sc):
    """... and without its bass's sequence (FRSh)"""
    return frsi(sc)[:OLD["FRSh"]["rec"]]


def as_old(sc, old):
    """a scene_t as an earlier shard kept it: its nv sounds, then the rest to its size"""
    return sc[:V + old["nv"]] + sc[V + NS:V + NS + old["size"] - V - old["nv"]]


def made_over(sc, bass=True, nv=ONV, scatter=True, tempo=TEMPO_DEF):
    """sc as an earlier layout's (nv sounds) comes in: the new settings at their defaults; its bass (its TEMPO tempo), or
    an empty one; GLINT's RELEASE where SCATTER was (scatter)"""
    b = sc[:V + nv] + bytes(p["def"] for p in S["params"][nv:NS]) + sc[V + NS:BN] + \
        (sc[BN:-1] + bytes([tempo]) if bass else EMPTY)
    return gone(b) if scatter else b


def b64(b):
    return base64.b64encode(b).decode()


def add(c, data, **kw):
    return c.post("/api/shards", json={"layout": LAYOUT, "firmware": "1.1", "data": b64(data), **kw})


def test_good_shards_from_the_example(client, scenes):
    signup(client)
    names = []
    for slot, sc in zip((2, 8, 9, 10), scenes):
        r = add(client, sc, from_slot=slot)
        assert r.status_code == 201, r.text
        j = r.json()
        names.append(j["name"])
        data = base64.b64decode(j["data"])
        assert len(data) == ST["size"] and data[ST["v"]:] == sc[ST["v"]:]
        assert j["root"] == sc[ST["v"] + IDS["P_ROOT"]] and j["scale"] == sc[ST["v"] + IDS["P_SCALE"]]
        assert j["type"] == sc[ST["v"] + IDS["P_SQ_TYPE"]] and j["drone"] == sc[ST["v"] + IDS["P_DR_PRESET"]]
        assert j["visibility"] == "private" and j["license"] == "CC BY 4.0" and j["from_slot"] == slot
        assert j["bseq"] == sc[ST["bq"]] and j["favorites"] == 0 and "title" not in j and "tags" not in j
    assert names == ["ABYSS", "CATHEDRAL", "GLACIER", "AURORA"]
    assert len(client.get("/api/shards").json()["items"]) == 4
    left = b"AURORA\x001".ljust(NL, b"Z") + scenes[3][NL:]   # (an older name's letters after the NUL, as a STORE
    assert base64.b64decode(add(client, left).json()["data"])[:NL] == b"AURORA".ljust(NL, b"\0")   # leaves them)
    full = add(client, b"SIXTEEN-LETTERS." + scenes[0][NL:]).json()            # (all sixteen used: no NUL)
    assert full["name"] == "SIXTEEN-LETTERS." and base64.b64decode(full["data"])[:NL] == b"SIXTEEN-LETTERS."


def test_bad_shards(client, scenes):
    signup(client)
    sc = scenes[0]
    capped = next(p for p in range(S["sizes"]["NSOUND"]) if S["params"][p]["max"] < 255)
    over = bytearray(sc)
    over[ST["v"] + capped] = S["params"][capped]["max"] + 1
    size = f"{ST['size']} bytes"

    def named(n):
        return n.ljust(NL, b"\0") + sc[NL:]
    for data, layout, why in ((sc[:-1], LAYOUT, size), (sc + b"\0", LAYOUT, size),
                              (bytes(over), LAYOUT, "out of range"), (named(b"bad name"), LAYOUT, "name"),
                              (named(b"A/B"), LAYOUT, "name"), (named(b""), LAYOUT, "name"),
                              (sc, "FRSf", "another FRASIL"), (sc[:BN], LAYOUT, size)):
        r = add(client, data, layout=layout)
        assert r.status_code == 422 and why in r.json()["detail"], (why, r.text)
    assert client.post("/api/shards", json={"layout": LAYOUT, "data": "not base64!"}).status_code == 422
    assert add(client, sc, from_slot=16).status_code == 422
    assert add(client, sc, firmware="1.1; DROP").status_code == 422
    assert add(client, sc, name="much too long a name").status_code == 422
    r = add(client, sc, name="X" * (NL + 1))
    assert r.status_code == 422 and f"1 to {NL} of A-Z" in r.json()["detail"]
    assert client.get("/api/shards").json()["items"] == []
    client.cookies.clear()
    assert add(client, sc).status_code == 401


def test_a_shard_keeps_its_bass(client, make, scenes):
    """AURORA's bass's sequence (SEQUENCE on, its notes and lengths): the shard's last bytes, as they came, after a
    rename, in the export's scene package"""
    signup(client)
    sc = scenes[3]
    assert sc[ST["bq"]] == 1 and any(sc[ST["bnote"]:ST["blen"]]) and sc[BN:] != EMPTY
    j = add(client, sc).json()
    assert j["layout"] == LAYOUT and j["name"] == "AURORA" and base64.b64decode(j["data"]) == sc and j["bseq"] == 1
    e = client.patch(f"/api/shards/{j['id']}", json={"name": "Dawn"}).json()
    assert base64.b64decode(e["data"])[NL:] == sc[NL:]
    assert base64.b64decode(client.get("/api/shards").json()["items"][0]["data"])[BN:] == sc[BN:]


def test_a_page_from_before(client, scenes):
    """a shard sent by a page still open from before (FRSi: before DECAY, SUSTAIN and PULSE; FRSh: no bass's sequence
    either; FRSg: names of ten too): kept in this layout, the new settings at their defaults, its bass as it came (FRSi)
    or empty"""
    signup(client)
    sc = scenes[3]
    assert len(frsi(sc)) == OLD["FRSi"]["size"] and len(frsh(sc)) == OLD["FRSh"]["size"] and LAYOUT not in OLD
    for data, layout, bass in ((frsi(sc), "FRSi", True), (frsh(sc), "FRSh", False),
                               (b"AURORA\0\0\0\0" + frsh(sc)[NL:], "FRSg", False)):
        j = add(client, data, layout=layout).json()
        assert j["layout"] == LAYOUT and j["name"] == "AURORA", (layout, j)
        assert base64.b64decode(j["data"]) == made_over(sc, bass) and j["bseq"] == int(bass), layout


def test_edit_rename_delete(client, make, scenes):
    signup(client)
    j = add(client, scenes[0], name="dark sea").json()
    assert j["name"] == "DARK SEA" and base64.b64decode(j["data"])[:NL] == b"DARK SEA".ljust(NL, b"\0")
    edit = {"name": "Deep #2", "title": "A deep drone", "tags": ["Dark", "dark", "sea"],
            "description": "Low.\n\n\n\nVery low."}
    r = client.patch(f"/api/shards/{j['id']}", json=edit)
    e = r.json()
    assert e["name"] == "DEEP #2" and base64.b64decode(e["data"])[:NL] == b"DEEP #2".ljust(NL, b"\0")
    assert base64.b64decode(e["data"])[NL:] == base64.b64decode(j["data"])[NL:]
    long = client.patch(f"/api/shards/{j['id']}", json={"name": "the deepest sea."}).json()   # (sixteen: no NUL)
    assert long["name"] == "THE DEEPEST SEA." and base64.b64decode(long["data"])[:NL] == b"THE DEEPEST SEA."
    assert e["description"] == "Low.\n\nVery low." and "title" not in e and "tags" not in e
    assert client.patch(f"/api/shards/{j['id']}", json={"name": "BAD/NAME"}).status_code == 422
    assert client.patch(f"/api/shards/{j['id']}", json={"description": "x" * 1001}).status_code == 422
    for ignored in ({"tags": [str(i) for i in range(9)]}, {"title": "x" * 81}, {"title": "porn", "tags": ["porn"]}):
        assert client.patch(f"/api/shards/{j['id']}", json=ignored).status_code == 200   # (v1's page: ignored)
    assert tuple(db(client).execute("SELECT title, tags FROM shards").fetchone()) == ("", "[]")
    b = other(client, make, email="bo@river.test", name="Bo")
    for r in (b.patch(f"/api/shards/{j['id']}", json={"title": "mine"}), b.delete(f"/api/shards/{j['id']}"),
              b.post(f"/api/shards/{j['id']}/share", json={})):
        assert r.status_code == 404
    assert b.get("/api/shards").json()["items"] == []
    assert client.delete(f"/api/shards/{j['id']}").json() == {"ok": True}
    assert client.get("/api/shards").json()["items"] == []


def test_share_needs_a_verified_email(client, scenes):
    signup(client)
    sid = add(client, scenes[1]).json()["id"]
    r = client.post(f"/api/shards/{sid}/share", json={})
    assert r.status_code == 403 and "Confirm your email" in r.json()["detail"]
    client.post("/api/auth/verify", json={"token": link("verify")})
    assert client.post(f"/api/shards/{sid}/share", json={"description": "fucking loud"}).status_code == 422
    r = client.post(f"/api/shards/{sid}/share", json={"title": "Fucking loud", "tags": ["choir"],   # (ignored)
                                                      "description": "Cathedral air"})
    j = r.json()
    assert r.status_code == 200 and j["visibility"] == "public" and j["shared_at"]
    assert j["description"] == "Cathedral air" and "title" not in j and "tags" not in j
    assert client.get(f"/api/public/shards/{sid}").status_code == 200
    assert client.patch(f"/api/shards/{sid}", json={"description": "porn"}).status_code == 422   # (public: shown)
    first = j["shared_at"]
    assert client.post(f"/api/shards/{sid}/unshare", json={}).json()["visibility"] == "private"
    assert client.get(f"/api/public/shards/{sid}").status_code == 404
    db(client).execute("UPDATE shards SET shared_at = shared_at - 100")
    again = client.post(f"/api/shards/{sid}/share", json={}).json()
    assert again["shared_at"] < first             # (its first sharing stays its date: no climbing back to the top)
    db(client).execute("UPDATE shards SET visibility = 'hidden'")
    assert client.post(f"/api/shards/{sid}/share", json={}).status_code == 403
    assert client.post(f"/api/shards/{sid}/unshare", json={}).json()["visibility"] == "hidden"


def test_backups(client, make, example):
    signup(client)
    r = client.post("/api/backups", json={"image": b64(example), "note": "before the gig", "firmware": "1.1"})
    assert r.status_code == 201, r.text
    j = r.json()
    assert j["size"] == len(example) and j["note"] == "before the gig"
    assert [n for n in j["scenes"] if n] == ["ABYSS", "CATHEDRAL", "GLACIER", "AURORA"] and len(j["scenes"]) == 16
    assert base64.b64decode(client.get(f"/api/backups/{j['id']}").json()["image"]) == example
    objs = {}                                     # (1.1's test builds': the settings and the scenes' record, no basses)
    pos = 16
    for _ in range(3):
        obj, ln = example[pos] | example[pos + 1] << 8, example[pos + 2] | example[pos + 3] << 8
        objs[obj], pos = example[pos + 8:pos + 8 + ln], pos + 8 + ln
    one = client.post("/api/backups", json={"image": b64(image([(0, objs[0]), (2, objs[2])])), "firmware": "1.1"}).json()
    assert one["scenes"] == j["scenes"] and len(objs[2]) == S["scenes_t"]["rec"]
    client.delete(f"/api/backups/{one['id']}")
    assert [x["id"] for x in client.get("/api/backups").json()["items"]] == [j["id"]]
    assert "image" not in client.get("/api/backups").json()["items"][0]
    b = other(client, make, email="bo@river.test", name="Bo")
    assert b.get(f"/api/backups/{j['id']}").status_code == 404
    assert b.delete(f"/api/backups/{j['id']}").status_code == 404
    assert client.delete(f"/api/backups/{j['id']}").json() == {"ok": True}
    assert client.get("/api/backups").json()["items"] == []


def test_bad_backups(client, example):
    signup(client)
    flip = bytearray(example)
    flip[100] ^= 1
    whole = bytearray(example)
    whole[-1] ^= 1
    for img, why in ((bytes(flip), "CRC-32"), (bytes(whole), "whole file"), (example[:-10], "damaged"),
                     (b"PLYB" + example[4:], "not a FRASIL backup"), (image([(0, b"x")], version=2), "another version"),
                     (image([(0, bytes(3000)), (2, bytes(3000)), (3, bytes(3000))]), "8192 bytes"),
                     (b"FRBK", "too short")):
        r = client.post("/api/backups", json={"image": b64(img)})
        assert r.status_code == 422 and why in r.json()["detail"], (why, r.text)
    old = client.post("/api/backups", json={"image": b64(image([(0, b""), (2, b"FRSf" + bytes(100))]))})
    assert old.status_code == 201 and old.json()["scenes"] is None    # (an earlier FRASIL's scenes: kept as they are)


def test_earlier_layout_made_over(client, make, scenes):
    """FRSg (names of ten) at startup: into this layout (FRSg -> FRSh -> FRSi -> this one), the name NUL-padded, the
    new settings at their defaults, the bass empty, the rest as it was; once"""
    old = next(x for x in S["old_scenes"] if x["magic"] == "FRSg")
    assert (old["nv"], old["name"], NL) == (ONV, 10, 16) and LAYOUT != "FRSg"
    signup(client, verified=True)
    ids = [add(client, sc).json()["id"] for sc in scenes[:4]]
    sid = client.post(f"/api/shards/{ids[0]}/share", json={"title": "Abyss"}).json()["id"]
    was = {sid: b"ABYSS\0\0\0\0\0" + frsh(scenes[0])[NL:], ids[1]: b"TENLETTERS" + frsh(scenes[1])[NL:],   # (all ten
           ids[2]: b"GLACIER\0AB" + frsh(scenes[2])[NL:]}                   # used: no NUL; an older name's letters)
    d = db(client)
    for i, data in was.items():
        d.execute("UPDATE shards SET layout = 'FRSg', data = ?, name = ? WHERE id = ?",
                  (data, data[:10].split(b"\0")[0].decode(), i))
    d.execute("UPDATE shards SET layout = 'FRSf', data = ? WHERE id = ?", (b"FRSf" + bytes(100), ids[3]))   # (not one)
    assert all(x["layout"] == "FRSg" for x in client.get("/api/shards").json()["items"] if x["id"] != ids[3])
    after = make()                                # (the service started again, on the same database)
    after.post("/api/auth/login", json={"email": "ana@river.test", "password": PASSWORD})
    by = {x["id"]: x for x in after.get("/api/shards").json()["items"]}
    for (i, data), sc in zip(was.items(), scenes):
        x = by[i]
        assert x["layout"] == LAYOUT and x["firmware"] == "1.1"
        new = base64.b64decode(x["data"])
        assert len(new) == ST["size"] and new[NL:] == made_over(sc, False)[NL:] and new[V + ONV:V + NS] == DEFS
        assert new[:NL] == data[:10].split(b"\0")[0].ljust(NL, b"\0") and x["bseq"] == 0
    assert (by[sid]["name"], by[ids[1]]["name"], by[ids[2]]["name"]) == ("ABYSS", "TENLETTERS", "GLACIER")
    assert by[ids[3]]["layout"] == "FRSf" and base64.b64decode(by[ids[3]]["data"]) == b"FRSf" + bytes(100)
    assert after.get(f"/api/public/shards/{sid}").json()["layout"] == LAYOUT
    rows = db(client).execute("SELECT id, layout, data, updated_at FROM shards ORDER BY id").fetchall()
    make()                                        # (again: nothing left to do)
    assert db(client).execute("SELECT id, layout, data, updated_at FROM shards ORDER BY id").fetchall() == rows


def test_bass_less_layout_made_over(client, make, scenes):
    """FRSh (a scene's record alone: shards made before the bass's sequence) at startup: this layout (FRSh -> FRSi ->
    this one), the same bytes, the new settings at their defaults, then an empty bass; once"""
    old = next(x for x in S["old_shards"] if x["size"] == x["rec"])
    assert old["magic"] == "FRSh" != LAYOUT
    signup(client)
    sid, keep = add(client, scenes[3]).json()["id"], add(client, scenes[0]).json()["id"]
    db(client).execute("UPDATE shards SET layout = ?, data = ? WHERE id = ?", (old["magic"], frsh(scenes[3]), sid))
    after = make()
    after.post("/api/auth/login", json={"email": "ana@river.test", "password": PASSWORD})
    by = {x["id"]: x for x in after.get("/api/shards").json()["items"]}
    assert by[sid]["layout"] == LAYOUT and by[sid]["name"] == "AURORA" and by[sid]["bseq"] == 0
    assert base64.b64decode(by[sid]["data"]) == made_over(scenes[3], False) != scenes[3]   # (AURORA's own bass: not in
    assert base64.b64decode(by[keep]["data"]) == scenes[0] and by[keep]["layout"] == LAYOUT   # FRSh)
    rows = db(client).execute("SELECT id, layout, data, updated_at FROM shards ORDER BY id").fetchall()
    make()                                        # (again: nothing left to do)
    assert db(client).execute("SELECT id, layout, data, updated_at FROM shards ORDER BY id").fetchall() == rows


def test_frsi_made_over(client, make, scenes):
    """FRSi (before BASS's DECAY, SUSTAIN and PULSE: the River's shards till then) at startup: this layout, the three
    inserted after the sound at their defaults (SUSTAIN 100 %, PULSE OFF), the rest and the bass's sequence as they
    were; a shard of this layout left alone; once"""
    assert OLD["FRSi"]["size"] == ST["size"] - (NS - ONV) - 1 and OLD["FRSi"]["rec"] == BN - (NS - ONV)
    assert DEFS[:3] == bytes((S["params"][ONV]["def"], 127, 0)) and S["params"][ONV + 1]["name"] == "SUSTAIN"
    signup(client)
    ids = [add(client, sc).json()["id"] for sc in scenes]
    for i, sc in zip(ids[:3], scenes):
        db(client).execute("UPDATE shards SET layout = 'FRSi', data = ? WHERE id = ?", (frsi(sc), i))
    after = make()
    after.post("/api/auth/login", json={"email": "ana@river.test", "password": PASSWORD})
    by = {x["id"]: x for x in after.get("/api/shards").json()["items"]}
    for i, sc in zip(ids, scenes):
        new = base64.b64decode(by[i]["data"])
        assert by[i]["layout"] == LAYOUT and new == made_over(sc) and new[BN:-1] == sc[BN:-1]
        assert by[i]["bseq"] == sc[ST["bq"]]
    assert base64.b64decode(by[ids[3]]["data"]) == scenes[3]
    rows = db(client).execute("SELECT id, layout, data, updated_at FROM shards ORDER BY id").fetchall()
    make()                                        # (again: nothing left to do)
    assert db(client).execute("SELECT id, layout, data, updated_at FROM shards ORDER BY id").fetchall() == rows


def test_frsk_made_over(client, make, scenes):
    """FRSk (SCATTER where GLINT's RELEASE is: the River's shards till then), sent by a page from before and at
    startup: this layout, RELEASE as the scene's DECAY let a glint go then (DECAY 127: 112), the rest as it was"""
    k = OLD["FRSk"]
    assert k["size"] == ST["size"] - (NS - k["nv"]) - 1 and k["rec"] == BN - (NS - k["nv"]) and GONE["of"][127] == 112
    assert S["params"][GONE["p"]]["name"] == "RELEASE" and GONE["p"] == IDS["P_GL_RELEASE"]
    signup(client)
    sc = bytearray(scenes[2])
    sc[V + GONE["p"]], sc[V + GONE["decay"]] = 99, 127
    sc = bytes(sc)
    old, want = as_old(sc, k), made_over(sc, True, k["nv"])
    j = add(client, old, layout="FRSk").json()
    assert j["layout"] == LAYOUT and base64.b64decode(j["data"]) == want
    assert base64.b64decode(j["data"])[V + GONE["p"]] == 112
    i = add(client, scenes[1]).json()["id"]
    db(client).execute("UPDATE shards SET layout = 'FRSk', data = ? WHERE id = ?", (old, i))
    after = make()
    after.post("/api/auth/login", json={"email": "ana@river.test", "password": PASSWORD})
    by = {x["id"]: x for x in after.get("/api/shards").json()["items"]}
    assert by[i]["layout"] == LAYOUT and base64.b64decode(by[i]["data"]) == want


def test_frsm_made_over(client, make, scenes):
    """FRSm (before the scene's TEMPO: the River's shards till then), sent by a page from before and at startup: this
    layout, its TEMPO at the default, its RELEASE as it was (no SCATTER to make over), the rest as it was; once"""
    m = OLD["FRSm"]
    assert m["size"] == ST["size"] - (NS - m["nv"]) - 1 and not m["scatter"] and ST["tempo"] == ST["size"] - 1
    signup(client)
    sc = bytearray(scenes[1])
    sc[V + GONE["p"]] = 33
    sc = bytes(sc)
    old, want = as_old(sc, m), made_over(sc, True, m["nv"], False)
    j = add(client, old, layout="FRSm").json()
    assert j["layout"] == LAYOUT and base64.b64decode(j["data"]) == want
    i = add(client, scenes[3]).json()["id"]
    db(client).execute("UPDATE shards SET layout = 'FRSm', data = ? WHERE id = ?", (old, i))
    after = make()
    after.post("/api/auth/login", json={"email": "ana@river.test", "password": PASSWORD})
    by = {x["id"]: x for x in after.get("/api/shards").json()["items"]}
    assert by[i]["layout"] == LAYOUT and base64.b64decode(by[i]["data"]) == want


def test_frsn_made_over(client, make, scenes):
    """FRSn (before MASTER EQ, MASTER FILTER and LOFI TAPE: the River's shards till then), sent by a page from before and
    at startup: this layout, the sixteen at their defaults (off), its TEMPO and the rest as they were"""
    n = OLD["FRSn"]
    assert n["size"] == ST["size"] - (NS - n["nv"]) and not n["scatter"] and n["nv"] == IDS["P_EQ_PRESET"]
    signup(client)
    sc = scenes[3]
    old, want = as_old(sc, n), made_over(sc, True, n["nv"], False, sc[-1])
    j = add(client, old, layout="FRSn").json()
    assert j["layout"] == LAYOUT and base64.b64decode(j["data"]) == want and j["bseq"] == sc[ST["bq"]]
    i = add(client, scenes[0]).json()["id"]
    db(client).execute("UPDATE shards SET layout = 'FRSn', data = ? WHERE id = ?", (old, i))
    after = make()
    after.post("/api/auth/login", json={"email": "ana@river.test", "password": PASSWORD})
    by = {x["id"]: x for x in after.get("/api/shards").json()["items"]}
    assert by[i]["layout"] == LAYOUT and base64.b64decode(by[i]["data"]) == want


def test_frsp_made_over(client, make, scenes):
    """FRSp (2.5's betas', before ROUTING and the pans), sent by a page from then and at startup: this layout, the seven
    at their defaults, its TEMPO and the rest as they were; GLINT DELAY's TIME of then's ping-pong half (19, 1/8D)
    within its twelve now (7)"""
    n, gd = OLD["FRSp"], IDS["P_GD_TIME"]
    assert n["size"] == ST["size"] - (NS - n["nv"]) and not n["scatter"] and n["nv"] == IDS["P_FL_PRE"]
    signup(client)
    sc = bytearray(scenes[3])
    sc[V + gd] = 7
    sc = bytes(sc)
    old, want = bytearray(as_old(sc, n)), made_over(sc, True, n["nv"], False, sc[-1])
    old[V + gd] = 19
    old = bytes(old)
    j = add(client, old, layout="FRSp").json()
    assert j["layout"] == LAYOUT and base64.b64decode(j["data"]) == want
    i = add(client, scenes[0]).json()["id"]
    db(client).execute("UPDATE shards SET layout = 'FRSp', data = ? WHERE id = ?", (old, i))
    after = make()
    after.post("/api/auth/login", json={"email": "ana@river.test", "password": PASSWORD})
    by = {x["id"]: x for x in after.get("/api/shards").json()["items"]}
    assert by[i]["layout"] == LAYOUT and base64.b64decode(by[i]["data"]) == want


def test_titles_and_tags_folded_at_startup(client, make, scenes):
    """from before the River v2 (titles, tags; no bseq, the column itself missing) at startup: a title becomes its
    description's first sentence (unless the description starts with it already; 1000 characters at most), titles and
    tags emptied, bseq read from data; once"""
    signup(client)
    ids = [add(client, sc).json()["id"] for sc in scenes + scenes[:3]]    # (AURORA, the fourth: its bass's sequence)
    was = (("Under the ice", "Two fifths in D phrygian.\nLet it rise.", '["dark", "slow"]'),
           ("Cathedral air!", "", "[]"), ("glacier", "Glacier, slow and white.", '["ice"]'), ("", "Dawn.", '["dawn"]'),
           ("Long", "x" * 1000, "[]"), ("Why not?", "Because.", "[]"), ("", "", "[]"))
    d = db(client)
    for i, (title, description, tags) in zip(ids, was):
        d.execute("UPDATE shards SET title = ?, description = ?, tags = ? WHERE id = ?", (title, description, tags, i))
    d.execute("ALTER TABLE shards DROP COLUMN bseq")
    after = make()                                # (the service started again, on the same database)
    after.post("/api/auth/login", json={"email": "ana@river.test", "password": PASSWORD})
    by = {x["id"]: x for x in after.get("/api/shards").json()["items"]}
    assert [by[i]["description"] for i in ids] == [
        "Under the ice. Two fifths in D phrygian.\nLet it rise.", "Cathedral air!", "Glacier, slow and white.", "Dawn.",
        "Long. " + "x" * 993 + "…", "Why not? Because.", ""]
    assert len(by[ids[4]]["description"]) == 1000 and [by[i]["bseq"] for i in ids] == [0, 0, 0, 1, 0, 0, 0]
    assert d.execute("SELECT COUNT(*) FROM shards WHERE title != '' OR tags != '[]'").fetchone()[0] == 0
    rows = d.execute("SELECT * FROM shards ORDER BY id").fetchall()
    make()                                        # (again: nothing left to do)
    assert db(client).execute("SELECT * FROM shards ORDER BY id").fetchall() == rows
