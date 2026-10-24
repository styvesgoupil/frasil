# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""What keeps the River safe: same origin and JSON for every change, the rate limits, the size of a request, the word
filter, the health route."""
import time
from collections import deque

import mail
from conftest import PASSWORD, signup
from guard import WORDS, filtered
from store import Settings
from test_shards import LAYOUT, S, add

FIRMWARE = S["firmware"]                         # (the schema's: VERSION, as make_schema.py wrote it)


def test_changes_come_from_the_site_as_json(client, scenes):
    signup(client)
    sid = add(client, scenes[0]).json()["id"]
    body = {"email": "x@river.test", "password": PASSWORD}
    for headers, code in (({"Origin": ""}, 403), ({"Origin": "https://evil.test"}, 403),
                          ({"Origin": "http://river.test"}, 403), ({"Origin": "https://river.test.evil.test"}, 403)):
        assert client.post("/api/auth/login", json=body, headers=headers).status_code == code
    r = client.post("/api/auth/login", content="email=x&password=y",
                    headers={"Content-Type": "application/x-www-form-urlencoded"})
    assert r.status_code == 415
    r = client.post("/api/auth/login", content="{}", headers={"Content-Type": "text/plain"})
    assert r.status_code == 415
    assert client.patch(f"/api/shards/{sid}", json={"title": "x"}, headers={"Origin": "https://evil.test"}
                        ).status_code == 403
    assert client.delete(f"/api/shards/{sid}", headers={"Origin": "https://evil.test"}).status_code == 403
    assert client.delete("/api/me", headers={"Origin": "null"}).status_code == 403
    assert client.get("/api/me", headers={"Origin": "https://evil.test"}).status_code == 200   # (reads: CORS)
    assert client.get("/api/shards").headers["cache-control"] == "no-store"
    assert client.delete(f"/api/shards/{sid}").status_code == 200       # (a DELETE: no body, no Content-Type)


def test_a_change_without_a_body(client):
    signup(client)
    assert client.post("/api/auth/logout", headers={"Content-Type": "application/json"}).json() == {"ok": True}
    assert client.get("/api/me").status_code == 401


def test_settings_from_the_environment(monkeypatch):
    for k, v in {"RIVER_URL": "https://river.example/", "RIVER_DB": "", "SMTP_PORT": "", "WORD_FILTER": "",
                 "ADMIN_EMAILS": " A@X.test, b@x.test ,", "COOKIE_SECURE": "0", "GOOGLE_REDIRECT_URI": ""}.items():
        monkeypatch.setenv(k, v)
    s = Settings.from_env()
    assert (s.url, s.db, s.smtp_port, s.admins, s.words, s.cookie_secure) == (
        "https://river.example", "river.sqlite", 465, ["a@x.test", "b@x.test"], None, False)
    assert s.google_redirect == "https://river.example/api/auth/google/callback" and s.ip_header == "cf-connecting-ip"


def test_too_big(client):
    r = client.post("/api/auth/login", content="{" + " " * 70000 + "}", headers={"Content-Type": "application/json"})
    assert r.status_code == 413


def test_login_limits(client):
    signup(client)
    for _ in range(10):
        assert client.post("/api/auth/login", json={"email": "ana@river.test", "password": "nope nope"}
                           ).status_code == 401
    r = client.post("/api/auth/login", json={"email": "ana@river.test", "password": PASSWORD})
    assert r.status_code == 429 and int(r.headers["retry-after"]) > 0       # 10 failures: for that email from there
    elsewhere = {"CF-Connecting-IP": "203.0.113.77"}                        # (and not a lock-out of its owner)
    assert client.post("/api/auth/login", json={"email": "ana@river.test", "password": PASSWORD},
                       headers=elsewhere).status_code == 200
    client.app.state.limiter.hits["login-fail:ana@river.test"].extend([time.monotonic()] * 40)   # 50 an hour, anywhere
    assert client.post("/api/auth/login", json={"email": "ana@river.test", "password": PASSWORD},
                       headers={"CF-Connecting-IP": "203.0.113.78"}).status_code == 429
    ip = {"CF-Connecting-IP": "203.0.113.5"}                                # by address, for any email
    for i in range(20):
        assert client.post("/api/auth/login", json={"email": f"x{i}@river.test", "password": "nope nope"},
                           headers=ip).status_code == 401
    assert client.post("/api/auth/login", json={"email": "y@river.test", "password": "nope nope"},
                       headers=ip).status_code == 429


def test_register_and_forgot_limits(client):
    ip = {"CF-Connecting-IP": "203.0.113.9"}
    for i in range(10):
        client.post("/api/auth/register", json={"email": f"u{i}@river.test", "password": PASSWORD,
                                                "display_name": "U"}, headers=ip)
    assert client.post("/api/auth/register", json={"email": "u10@river.test", "password": PASSWORD,
                                                   "display_name": "U"}, headers=ip).status_code == 429
    for _ in range(3):
        assert client.post("/api/auth/forgot", json={"email": "u1@river.test"}).status_code == 200
    assert client.post("/api/auth/forgot", json={"email": "u1@river.test"}).status_code == 429
    for i in range(2):
        client.post("/api/auth/forgot", json={"email": f"z{i}@river.test"})
    assert client.post("/api/auth/forgot", json={"email": "z9@river.test"}).status_code == 429   # (5 from an IP)


def test_upload_limit(client, scenes):
    signup(client)
    uid = client.get("/api/me").json()["id"]
    client.app.state.limiter.hits[f"upload:{uid}"] = deque([time.monotonic()] * 199)    # (200 an hour)
    assert add(client, scenes[0]).status_code == 201
    assert add(client, scenes[0]).status_code == 429


def test_odd_numbers(client, make):
    for q in ({"root": "²"}, {"page": 10 ** 20}, {"page": 0}):
        assert client.get("/api/public/shards", params=q).status_code == 422
    signup(client, email="admin@river.test", verified=True)
    assert client.post(f"/api/admin/users/{10 ** 20}/ban", json={}).status_code == 422
    assert client.post("/api/admin/users/12345/ban", json={}).status_code == 404


def test_emails_carry_nothing_typed(client):
    signup(client, name="Visit evil.test")
    assert "evil" not in mail.SENT[-1]["body"]
    for bad in ("a,victim@x.test", "a;b@x.test", "<a@x.test>", "a b@x.test", '"a"@x.test'):
        r = client.post("/api/auth/register", json={"email": bad, "password": PASSWORD, "display_name": "A"})
        assert r.status_code == 422, bad


def test_word_filter(make):
    assert filtered(WORDS, "Spicy night", "Scunthorpe drone", "") is None
    assert filtered(WORDS, "a calm one", "FUCKING loud") == "fuck"
    assert filtered(["glacier"], "GLACIERS") == "glacier" and filtered([], "anything") is None
    c = make(words=["glacier"])
    assert c.post("/api/auth/register", json={"email": "a@river.test", "password": PASSWORD,
                                              "display_name": "Glacier"}).status_code == 422
    assert c.post("/api/auth/register", json={"email": "a@river.test", "password": PASSWORD,
                                              "display_name": "Fucking"}).status_code == 201   # (the list replaced)


def test_health(client):
    assert client.get("/api/health").json() == {"ok": True, "layout": LAYOUT, "firmware": FIRMWARE}
