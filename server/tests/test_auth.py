# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""Accounts: register, login, logout, the session's cookie, verify, reset, the account's edits, bans, Google."""
import base64
import hashlib
import json
from urllib.parse import parse_qs, urlsplit

import httpx

import mail
from auth import COOKIE
from conftest import ADMIN, ORIGIN, PASSWORD, db, link, other, signup
from store import now


def cookie_of(r):
    return next(v for k, v in r.headers.multi_items() if k == "set-cookie" and v.startswith(COOKIE + "="))


def test_register_signs_in_and_sends_a_link(client):
    r = client.post("/api/auth/register", json={"email": " Ana@River.test ", "password": PASSWORD,
                                                 "display_name": "  Ana  "})
    assert r.status_code == 201
    assert r.json() | {"id": 0, "created_at": ""} == {"id": 0, "email": "ana@river.test", "display_name": "Ana",
                                                      "verified": False, "has_password": True, "is_admin": False,
                                                      "created_at": ""}
    c = cookie_of(r).lower()
    assert all(x in c for x in ("httponly", "secure", "samesite=lax", "path=/api", f"max-age={90 * 86400}"))
    assert client.get("/api/me").json()["email"] == "ana@river.test"
    assert mail.SENT[-1]["to"] == "ana@river.test" and f"{ORIGIN}/#verify=" in mail.SENT[-1]["body"]
    stored = db(client).execute("SELECT password FROM users").fetchone()[0]
    assert stored.startswith("$argon2id$")


def test_register_refuses(client):
    ok = {"email": "ana@river.test", "password": PASSWORD, "display_name": "Ana"}
    for bad, code in (({"email": "nope"}, 422), ({"password": "short"}, 422), ({"display_name": " "}, 422),
                      ({"display_name": "fucking ana"}, 422)):
        assert client.post("/api/auth/register", json=ok | bad).status_code == code
    assert client.post("/api/auth/register", json=ok).status_code == 201
    assert client.post("/api/auth/register", json=ok | {"email": "ANA@river.test"}).status_code == 409


def test_login_logout(client, make):
    signup(client)
    client.post("/api/auth/logout", json={})
    assert client.get("/api/me").status_code == 401
    for email, pw in (("ana@river.test", "wrong password"), ("nobody@river.test", PASSWORD)):
        r = client.post("/api/auth/login", json={"email": email, "password": pw})
        assert r.status_code == 401 and r.json()["detail"] == "Wrong email or password."
    assert client.post("/api/auth/login", json={"email": "ANA@river.test", "password": PASSWORD}).status_code == 200
    assert client.get("/api/me").status_code == 200
    client.post("/api/auth/login", json={"email": "ana@river.test", "password": PASSWORD})
    assert db(client).execute("SELECT COUNT(*) FROM sessions").fetchone()[0] == 1   # (the browser's last one ended)
    d = make(app=client.app)                     # another browser
    d.post("/api/auth/login", json={"email": "ana@river.test", "password": PASSWORD})
    client.post("/api/auth/logout", json={"all": True})
    assert d.get("/api/me").status_code == 401


def test_sessions_hashed_sliding_and_expiring(client):
    r = client.post("/api/auth/register", json={"email": "ana@river.test", "password": PASSWORD,
                                                 "display_name": "Ana"})
    token = cookie_of(r).split(";")[0].split("=", 1)[1]
    d = db(client)
    (h, exp), = d.execute("SELECT hash, expires_at FROM sessions").fetchall()
    assert h == hashlib.sha256(token.encode()).hexdigest() and len(token) >= 43 and token not in h
    assert abs(exp - (now() + 90 * 86400)) < 5
    assert "set-cookie" not in client.get("/api/me").headers   # (renewed once a day at most)
    d.execute("UPDATE sessions SET expires_at = ?", (now() + 10 * 86400,))
    r = client.get("/api/me")
    assert r.status_code == 200 and COOKIE in r.headers["set-cookie"]
    assert d.execute("SELECT expires_at FROM sessions").fetchone()[0] >= now() + 90 * 86400 - 5
    d.execute("UPDATE sessions SET expires_at = ?", (now() - 1,))
    assert client.get("/api/me").status_code == 401


def test_verify(client, make):
    signup(client)
    t = link("verify")
    assert client.post("/api/auth/verify", json={"token": "x" + t}).status_code == 400
    assert make(app=client.app).post("/api/auth/verify", json={"token": t}).status_code == 401   # (signed in only:
    b = other(client, make, email="bo@river.test", name="Bo")                                      # as that account)
    assert b.post("/api/auth/verify", json={"token": t}).status_code == 400
    assert client.post("/api/auth/verify", json={"token": t}).json() == {"ok": True, "verified": True}
    assert client.get("/api/me").json()["verified"] is True
    assert client.post("/api/auth/verify", json={"token": t}).status_code == 400          # (once)


def test_verify_link_expires_in_48_hours(client):
    signup(client)
    d = db(client)
    assert abs(d.execute("SELECT expires_at FROM tokens").fetchone()[0] - (now() + 48 * 3600)) < 5
    d.execute("UPDATE tokens SET expires_at = ?", (now() - 1,))
    assert client.post("/api/auth/verify", json={"token": link("verify")}).status_code == 400


def test_resend(client):
    assert client.post("/api/auth/resend", json={}).status_code == 401
    signup(client)
    for _ in range(3):
        assert client.post("/api/auth/resend", json={}).json() == {"ok": True, "verified": False}
    assert len(mail.SENT) == 4
    assert client.post("/api/auth/resend", json={}).status_code == 429
    client.post("/api/auth/verify", json={"token": link("verify")})
    assert client.post("/api/auth/resend", json={}).json()["verified"] is True


def test_forgot_and_reset(client, make):
    signup(client)
    assert client.post("/api/auth/forgot", json={"email": "nobody@river.test"}).json() == {"ok": True}
    assert len(mail.SENT) == 1                    # (the same answer; no email)
    assert client.post("/api/auth/forgot", json={"email": "ana@river.test"}).json() == {"ok": True}
    t = link("reset")
    assert abs(db(client).execute("SELECT expires_at FROM tokens WHERE kind = 'reset'").fetchone()[0] -
               (now() + 3600)) < 5
    assert client.post("/api/auth/reset", json={"token": t, "password": "short"}).status_code == 422
    d = make(app=client.app)
    r = d.post("/api/auth/reset", json={"token": t, "password": "a new password"})
    assert r.status_code == 200 and r.json()["verified"] is True     # (the link came to that email)
    assert client.get("/api/me").status_code == 401                  # every other session ended
    assert d.get("/api/me").status_code == 200
    assert d.post("/api/auth/reset", json={"token": t, "password": "another one"}).status_code == 400
    assert d.post("/api/auth/login", json={"email": "ana@river.test", "password": PASSWORD}).status_code == 401
    assert d.post("/api/auth/login", json={"email": "ana@river.test", "password": "a new password"}).status_code == 200


def test_reset_link_expires_in_an_hour(client):
    signup(client)
    client.post("/api/auth/forgot", json={"email": "ana@river.test"})
    db(client).execute("UPDATE tokens SET expires_at = ? WHERE kind = 'reset'", (now() - 1,))
    r = client.post("/api/auth/reset", json={"token": link("reset"), "password": "a new password"})
    assert r.status_code == 400


def test_me_edit(client, make):
    signup(client)
    assert client.patch("/api/me", json={"display_name": "Ana B."}).json()["display_name"] == "Ana B."
    assert client.patch("/api/me", json={"display_name": "shit"}).status_code == 422
    d = make(app=client.app)
    d.post("/api/auth/login", json={"email": "ana@river.test", "password": PASSWORD})
    assert client.patch("/api/me", json={"password": "wrong", "new_password": "a new password"}).status_code == 403
    assert client.patch("/api/me", json={"password": PASSWORD, "new_password": "a new password"}).status_code == 200
    assert client.get("/api/me").status_code == 200 and d.get("/api/me").status_code == 401


def test_ban(client, make):
    signup(client)
    uid = client.get("/api/me").json()["id"]
    a = other(client, make, email=ADMIN, name="Admin", verified=True)
    assert a.get("/api/me").json()["is_admin"] is True
    assert client.post(f"/api/admin/users/{uid}/ban", json={}).status_code == 403   # (not an admin)
    assert a.post(f"/api/admin/users/{uid}/ban", json={}).json() == {"ok": True, "banned": True}
    assert client.get("/api/me").status_code == 401
    r = client.post("/api/auth/login", json={"email": "ana@river.test", "password": PASSWORD})
    assert r.status_code == 403 and "banned" in r.json()["detail"]
    client.post("/api/auth/forgot", json={"email": "ana@river.test"})
    assert link("reset") is None
    aid = a.get("/api/me").json()["id"]
    assert a.post(f"/api/admin/users/{aid}/ban", json={}).status_code == 403          # (an admin: never)
    assert a.post(f"/api/admin/users/{uid}/unban", json={}).json()["banned"] is False
    assert client.post("/api/auth/login", json={"email": "ana@river.test", "password": PASSWORD}).status_code == 200


def test_admin_needs_a_verified_email(client):
    signup(client, email=ADMIN)                   # (anyone could register it: it counts once proved)
    assert client.get("/api/me").json()["is_admin"] is False
    assert client.get("/api/admin/reports").status_code == 403
    client.post("/api/auth/verify", json={"token": link("verify")})
    assert client.get("/api/admin/reports").status_code == 200


# ------------------------------------------------------------------------------------------------ Google
def id_token(**claims):
    enc = lambda d: base64.urlsafe_b64encode(json.dumps(d).encode()).rstrip(b"=").decode()  # noqa: E731
    return f"{enc({'alg': 'RS256'})}.{enc(claims)}.c2lnbmF0dXJl"


def google(make, claims=None, status=200, **kw):
    """a client whose service talks to a mocked Google: claims(nonce) -> the ID token's"""
    c = make(google_id="cid.apps.googleusercontent.com", google_secret="s3cret",
             google_redirect=f"{ORIGIN}/api/auth/google/callback", **kw)
    c.calls = []

    def token_endpoint(req):
        form = parse_qs(req.content.decode())
        c.calls.append(form)
        nonce = c.started["nonce"][0]
        cl = {"iss": "https://accounts.google.com", "aud": "cid.apps.googleusercontent.com", "exp": now() + 600,
              "nonce": nonce, "email": "gina@gmail.com", "email_verified": True, "name": "Gina"}
        cl.update(claims(nonce) if claims else {})
        return httpx.Response(status, json={"access_token": "at", "id_token": id_token(**cl)})

    c.app.state.http = httpx.Client(transport=httpx.MockTransport(token_endpoint))
    return c


def google_signin(c, next="/library/"):
    r = c.get("/api/auth/google/start", params={"next": next})
    assert r.status_code == 302
    url = urlsplit(r.headers["location"])
    c.started = parse_qs(url.query)
    return c.get("/api/auth/google/callback", params={"code": "the-code", "state": c.started["state"][0]})


def test_google_signs_up_with_pkce(make):
    c = google(make)
    r = google_signin(c)
    q = c.started
    assert q["code_challenge_method"] == ["S256"] and q["redirect_uri"] == [f"{ORIGIN}/api/auth/google/callback"]
    assert q["scope"] == ["openid email profile"] and q["client_id"] == ["cid.apps.googleusercontent.com"]
    assert r.status_code == 303 and r.headers["location"] == "/library/"
    sent = c.calls[0]
    assert sent["code"] == ["the-code"] and sent["grant_type"] == ["authorization_code"]
    challenge = base64.urlsafe_b64encode(hashlib.sha256(sent["code_verifier"][0].encode()).digest()).rstrip(b"=")
    assert challenge.decode() == q["code_challenge"][0]
    me = c.get("/api/me").json()
    assert [me[k] for k in ("email", "verified", "has_password", "display_name")] == ["gina@gmail.com", True, False,
                                                                                      "Gina"]
    assert db(c).execute("SELECT COUNT(*) FROM oauth").fetchone()[0] == 0     # (the state: once)


def test_google_refuses(make):
    for claims in (lambda n: {"nonce": "another"}, lambda n: {"aud": "someone-else"}, lambda n: {"exp": now() - 5},
                   lambda n: {"email_verified": False}, lambda n: {"iss": "https://evil.test"}):
        c = google(make, claims)
        r = google_signin(c)
        assert r.headers["location"] == "/#google=failed" and c.get("/api/me").status_code == 401
    c = google(make, status=400)
    assert google_signin(c).headers["location"] == "/#google=failed"
    c = google(make)                              # a state that isn't this browser's
    r = c.get("/api/auth/google/start")
    c.started = parse_qs(urlsplit(r.headers["location"]).query)
    c.cookies.clear()
    r = c.get("/api/auth/google/callback", params={"code": "x", "state": c.started["state"][0]})
    assert r.headers["location"] == "/#google=failed" and not c.calls
    assert google_signin(google(make), next="//evil.test/").headers["location"] == "/"


def test_google_takes_over_an_unproved_account(make):
    c = google(make)
    squat = make(app=c.app)
    signup(squat, email="gina@gmail.com", name="Not Gina")     # (made with Gina's email, never verified)
    google_signin(c)
    me = c.get("/api/me").json()
    assert me["verified"] and not me["has_password"] and me["display_name"] == "Not Gina"
    assert squat.get("/api/me").status_code == 401
    assert squat.post("/api/auth/login", json={"email": "gina@gmail.com", "password": PASSWORD}).status_code == 401


def test_google_banned_and_not_set_up(make):
    c = google(make)
    google_signin(c)
    db(c).execute("UPDATE users SET banned_at = ?", (now(),))
    assert google_signin(c).headers["location"] == "/#google=banned"
    assert make().get("/api/auth/google/start").status_code == 404


def test_providers_says_whether_google_is_set_up(client, make):
    assert client.get("/api/auth/providers").json() == {"password": True, "google": False}
    c = google(make)
    assert c.get("/api/auth/providers").json() == {"password": True, "google": True}


def test_session_says_who_without_a_401(client):
    r = client.get("/api/auth/session")
    assert r.status_code == 200 and r.json() == {"me": None, "password": True, "google": False}
    signup(client, email="sam@river.test", name="Sam")
    me = client.get("/api/auth/session").json()["me"]
    assert me["display_name"] == "Sam" and me["email"] == "sam@river.test" and me["verified"] is False
