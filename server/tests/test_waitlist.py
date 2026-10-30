# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""The waitlists (FRASIL for iPad, FRASIL VST): an email once per list, from the River or the main site; and the admin
page's routes: 404 to anyone without the key, the accounts, their uploads and the lists to whoever has it."""
import sqlite3

from conftest import ORIGIN, db, other, signup
from store import init
from test_public import shared
from test_shards import add, b64

KEY = "fog over the lake, 7 a.m."
SITE = "https://frasilsynth.com"


def join(c, email, list="ipad", **kw):
    return c.post("/api/waitlist", json={"list": list, "email": email}, **kw)


def rows(c):
    return [tuple(x) for x in db(c).execute("SELECT list, email FROM waitlist ORDER BY id")]


def test_join(client):
    r = join(client, "  Ana@River.Test ")
    assert r.status_code == 200 and r.json() == {"ok": True}
    assert join(client, "bo@river.test", "vst").json() == {"ok": True}
    assert rows(client) == [("ipad", "ana@river.test"), ("vst", "bo@river.test")]
    t = db(client).execute("SELECT created FROM waitlist").fetchone()[0]
    assert isinstance(t, int) and t > 1_700_000_000


def test_twice_is_once(client):
    for e in ("ana@river.test", "ANA@river.test", " ana@river.test"):
        r = join(client, e)
        assert r.status_code == 200 and r.json() == {"ok": True}
    assert join(client, "ana@river.test", "vst").status_code == 200     # (another list: another row)
    assert rows(client) == [("ipad", "ana@river.test"), ("vst", "ana@river.test")]


def test_not_a_list(client):
    for name in ("", "IPAD", "android", "ipad ", None, 3):
        r = client.post("/api/waitlist", json={"list": name, "email": "ana@river.test"})
        assert r.status_code == 422, name
    assert client.post("/api/waitlist", json={"email": "ana@river.test"}).status_code == 422
    assert rows(client) == []


def test_bad_emails(client):
    long = "a" * 245 + "@river.test"                                        # (255 characters)
    for e in ("", "ana", "ana@", "@river.test", "ana@river", "ana@river.", "ana@.test", "ana@@river.test",
              "ana@bo@river.test", "ana bo@river.test", "ana@river..test", "<ana@river.test>", "ana@river.test\x00",
              "ana,bo@river.test", long):
        assert join(client, e).status_code == 422, e
    assert rows(client) == []
    assert join(client, "a" * 243 + "@river.test").status_code == 200      # (254: the most)


def test_from_the_main_site_or_the_river_only(make):
    c = make(site_url=SITE)
    assert join(c, "ana@river.test", headers={"Origin": SITE}).status_code == 200
    assert join(c, "bo@river.test", headers={"Origin": ORIGIN}).status_code == 200
    for o in ("https://evil.test", "", "http://frasilsynth.com"):
        assert join(c, "cy@river.test", headers={"Origin": o}).status_code == 403
    r = c.post("/api/waitlist", content="list=ipad&email=cy@river.test", headers={
        "Origin": SITE, "Content-Type": "application/x-www-form-urlencoded"})
    assert r.status_code == 415
    # the main site's Origin is good for the waitlist only
    r = c.post("/api/auth/login", json={"email": "ana@river.test", "password": "x" * 9}, headers={"Origin": SITE})
    assert r.status_code == 403
    assert len(rows(c)) == 2


def test_rate_limited(client):
    for i in range(20):
        assert join(client, f"n{i}@river.test").status_code == 200
    r = join(client, "late@river.test")
    assert r.status_code == 429 and "retry-after" in r.headers


def test_an_older_database_gets_the_table(tmp_path):
    p = str(tmp_path / "old.sqlite")
    old = sqlite3.connect(p)
    old.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, email TEXT NOT NULL UNIQUE, display_name TEXT NOT NULL, "
                "password TEXT, created_at INTEGER NOT NULL, verified_at INTEGER, banned_at INTEGER)")
    old.execute("INSERT INTO users (email, display_name, created_at) VALUES ('ana@river.test', 'Ana', 1)")
    old.commit()
    old.close()
    init(p)
    d = sqlite3.connect(p)
    assert d.execute("SELECT COUNT(*) FROM users").fetchone()[0] == 1
    assert [c[1] for c in d.execute("PRAGMA table_info(waitlist)")] == ["id", "list", "email", "created"]


# ---------------------------------------------------------------------------------------------- the admin page's
ROUTES = ("/api/admin/overview", "/api/admin/users/1", "/api/admin/waitlist", "/api/admin/users/x")


def test_admin_without_the_key_set_is_404(make):
    c = make()                                                            # (no FRASIL_ADMIN_KEY)
    signup(c)
    for p in ROUTES:
        for h in ({}, {"X-Frasil-Admin": ""}, {"X-Frasil-Admin": KEY}):
            r = c.get(p, headers=h)
            assert r.status_code == 404 and r.json() == {"detail": "Not Found"}, (p, h)


def test_admin_with_a_wrong_key_is_404(make):
    c = make(admin_key=KEY)
    signup(c)
    for p in ROUTES:
        for h in ({}, {"X-Frasil-Admin": ""}, {"X-Frasil-Admin": KEY[:-1]}, {"X-Frasil-Admin": KEY + " "},
                  {"X-Frasil-Admin": KEY.upper()}, {"X-Frasil-Admin-Key": KEY}):
            r = c.get(p, headers=h)
            assert r.status_code == 404 and r.json() == {"detail": "Not Found"}, (p, h)
            assert KEY not in r.text


def test_admin_sees_accounts_uploads_and_lists(make, scenes, example):
    c = make(admin_key=KEY)
    a = signup(c, verified=True)
    pub = shared(c, scenes[0], description="Abyss, deep.")
    priv = add(c, scenes[1]).json()["id"]
    assert c.post("/api/backups", json={"image": b64(example), "note": "gig"}).status_code == 201
    bo = other(c, make, email="bo@river.test", name="Bo")
    assert bo.post(f"/api/favorites/{pub}", json={}).status_code == 201
    db(c).execute("UPDATE users SET password = NULL WHERE email = 'bo@river.test'")   # (as Google makes one)
    join(c, "zed@river.test")
    join(c, "amy@river.test")
    join(c, "amy@river.test", "vst")
    db(c).execute("UPDATE waitlist SET created = created - 60 WHERE email = 'zed@river.test'")
    h = {"X-Frasil-Admin": KEY}

    o = c.get("/api/admin/overview", headers=h)
    assert o.status_code == 200 and o.headers["cache-control"] == "no-store"
    o = o.json()
    assert [u["email"] for u in o["users"]] == ["bo@river.test", "ana@river.test"]       # (the newest first)
    ana, b = o["users"][1], o["users"][0]
    assert ana["id"] == a["id"]
    assert (ana["display_name"], ana["verified"], ana["sign_in"], ana["banned"]) == ("Ana", True, "password", False)
    assert ana["shards"] == {"public": 1, "private": 1, "hidden": 0, "total": 2} and ana["backups"] == 1
    assert (b["sign_in"], b["verified"], b["favorites"], b["shards"]["total"]) == ("google", False, 1, 0)
    assert ana["created_at"].endswith("Z") and ana["verified_at"]
    assert o["totals"] == {"users": 2, "verified": 1, "shards": 2, "public": 1, "backups": 1}
    assert o["waitlist"] == {"ipad": 2, "vst": 1}
    assert "password" not in o["users"][0] and "$argon2" not in str(o)

    u = c.get(f"/api/admin/users/{ana['id']}", headers=h).json()
    assert u["user"]["email"] == "ana@river.test"
    assert {s["id"]: (s["public"], s["visibility"]) for s in u["shards"]} == {pub: (True, "public"),
                                                                               priv: (False, "private")}
    one = next(s for s in u["shards"] if s["id"] == pub)
    assert (one["name"], one["description"], one["favorites"], one["plays"]) == ("ABYSS", "Abyss, deep.", 1, 0)
    assert "data" not in one
    (bk,) = u["backups"]
    assert (bk["note"], bk["size"]) == ("gig", len(example)) and "image" not in bk
    assert c.get("/api/admin/users/999", headers=h).status_code == 404
    assert c.get("/api/admin/users/x", headers=h).status_code == 422

    w = c.get("/api/admin/waitlist", headers=h).json()["lists"]
    assert w["ipad"]["count"] == 2 and [x["email"] for x in w["ipad"]["items"]] == ["amy@river.test", "zed@river.test"]
    assert w["vst"] == {"count": 1, "items": [{"email": "amy@river.test", "created_at": w["vst"]["items"][0]["created_at"]}]}


def test_the_key_is_never_shown(make):
    c = make(admin_key=KEY)
    assert KEY not in repr(c.app.state.settings)
