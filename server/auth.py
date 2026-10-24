# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""Accounts: an email and a password (argon2id), or Continue with Google (OAuth 2.0's authorization code, with PKCE,
a state bound to the browser by a cookie and a nonce); the session's cookie (a random 256-bit token: only its
SHA-256 kept; HttpOnly, Secure, SameSite=Lax, 90 days, renewed as it is used); the email's verification (48 h) and
the password's reset (1 h), each a one-time link to the page (#verify=, #reset=: the token never reaches a log).
The account itself: GET/PATCH/DELETE /api/me (its export: shards.py)."""
import base64
import hashlib
import json
import re
import secrets
import sqlite3
import threading
import time
from urllib.parse import urlencode

import httpx
from argon2 import PasswordHasher
from argon2.exceptions import InvalidHashError, VerificationError
from fastapi import APIRouter, BackgroundTasks, Depends, HTTPException, Request, Response
from fastapi.responses import RedirectResponse
from pydantic import BaseModel

import mail
import store
from guard import clean, client_ip, filtered, WORDS
from store import iso, now, tx

COOKIE, OAUTH_COOKIE = "river_session", "river_oauth"
SESSION_DAYS, VERIFY_H, RESET_H = 90, 48, 1
BANNED = "This account is banned from the River."
EMAIL = re.compile(r"^[A-Za-z0-9._%+'-]+@[A-Za-z0-9-]+(\.[A-Za-z0-9-]+)*\.[A-Za-z]{2,}$")   # (no , ; < > " spaces)
ph = PasswordHasher()                             # argon2id, argon2-cffi's defaults (RFC 9106's low-memory choice)
HASHING = threading.BoundedSemaphore(4)           # (64 MiB each: 4 at a time at most)
DUMMY = ph.hash("not anyone's password")          # (a login for no one takes as long as a wrong password)
r = APIRouter(prefix="/api")


def sha(t):
    return hashlib.sha256(t.encode()).hexdigest()


# ---------------------------------------------------------------------------------------------- the request's
def get_db(request: Request):
    app = request.app
    db = store.connect(app.state.settings.db)
    try:
        if time.monotonic() - app.state.kept > 3600:
            app.state.kept = time.monotonic()
            store.housekeep(db)
        yield db
    finally:
        db.close()


DB = Depends(get_db)


def session_of(request, db):
    t = request.cookies.get(COOKIE)
    if not t:
        return None
    return db.execute("SELECT u.*, s.hash, s.expires_at FROM sessions s JOIN users u ON u.id = s.user_id "
                      "WHERE s.hash = ? AND s.expires_at > ?", (sha(t), now())).fetchone()


def maybe_user(request: Request, response: Response, db=DB):
    """the signed-in user, or None; the session renewed (once a day at most: 90 days from its last use)"""
    u = session_of(request, db)
    if u and u["banned_at"]:
        db.execute("DELETE FROM sessions WHERE user_id = ?", (u["id"],))
        return None
    if u and u["expires_at"] < now() + (SESSION_DAYS - 1) * 86400:
        db.execute("UPDATE sessions SET expires_at = ? WHERE hash = ?", (now() + SESSION_DAYS * 86400, u["hash"]))
        set_cookie(response, request.cookies[COOKIE], request.app.state.settings)
    return u


def user(u=Depends(maybe_user)):
    if not u:
        raise HTTPException(401, "Sign in first.")
    return u


def admin(request: Request, u=Depends(user)):
    if not is_admin(u, request.app.state.settings):
        raise HTTPException(403, "For the River's admins only.")
    return u


def is_admin(u, s):
    return bool(u["verified_at"]) and u["email"] in s.admins


def set_cookie(response, token, s):
    response.set_cookie(COOKIE, token, max_age=SESSION_DAYS * 86400, path="/api", secure=s.cookie_secure,
                        httponly=True, samesite="lax")


def start_session(db, uid, response, request):
    """a new session (the one this browser had, if any, ends: no session fixed beforehand)"""
    db.execute("DELETE FROM sessions WHERE hash = ?", (sha(request.cookies.get(COOKIE, "")),))
    t = secrets.token_urlsafe(32)
    db.execute("INSERT INTO sessions (hash, user_id, created_at, expires_at) VALUES (?, ?, ?, ?)",
               (sha(t), uid, now(), now() + SESSION_DAYS * 86400))
    set_cookie(response, t, request.app.state.settings)


def me_json(u, s):
    return {"id": u["id"], "email": u["email"], "display_name": u["display_name"], "verified": bool(u["verified_at"]),
            "has_password": bool(u["password"]), "is_admin": is_admin(u, s), "created_at": iso(u["created_at"])}


def fresh(db, uid):
    return db.execute("SELECT * FROM users WHERE id = ?", (uid,)).fetchone()


# ---------------------------------------------------------------------------------------------- what is typed
def norm_email(e):
    e = str(e or "").strip().lower()
    if len(e) > 254 or not EMAIL.match(e):
        raise HTTPException(422, "Enter a valid email address.")
    return e


def password_ok(p):
    if not 8 <= len(p or "") <= 256:
        raise HTTPException(422, "A password: 8 characters at least.")
    return p


def display_name(name, s):
    n = clean(name, 40, "A display name")
    if not n:
        raise HTTPException(422, "Choose a display name.")
    if filtered(s.words or WORDS, n):
        raise HTTPException(422, "That display name has a word the River doesn't take.")
    return n


def hashed(password):
    with HASHING:
        return ph.hash(password)


def check_password(stored, password):
    try:
        with HASHING:
            return ph.verify(stored or DUMMY, password) and bool(stored)
    except (VerificationError, InvalidHashError):
        return False


def make_token(db, uid, kind, hours):
    t = secrets.token_urlsafe(32)
    db.execute("INSERT INTO tokens (hash, user_id, kind, expires_at) VALUES (?, ?, ?, ?)",
               (sha(t), uid, kind, now() + hours * 3600))
    return t


def use_token(db, t, kind, uid=None):
    """a one-time link's token (uid's only, if given) -> its user's id (and it is spent), or None"""
    rows = db.execute("DELETE FROM tokens WHERE hash = ? AND kind = ? AND expires_at > ? AND user_id = "
                      "COALESCE(?, user_id) RETURNING user_id", (sha(str(t or "")), kind, now(), uid)).fetchall()
    return rows[0][0] if rows else None


def send_verify(db, u, tasks, s):
    t = make_token(db, u["id"], "verify", VERIFY_H)
    tasks.add_task(mail.verify, s, u["email"], f"{s.url}/#verify={t}")


# ---------------------------------------------------------------------------------------------- email + password
class Register(BaseModel):
    email: str
    password: str
    display_name: str


class Login(BaseModel):
    email: str
    password: str


class Token(BaseModel):
    token: str


class Email(BaseModel):
    email: str


class Reset(BaseModel):
    token: str
    password: str


class Logout(BaseModel):
    all: bool = False


@r.post("/auth/register", status_code=201)
def register(body: Register, request: Request, response: Response, tasks: BackgroundTasks, db=DB):
    s, lim = request.app.state.settings, request.app.state.limiter
    lim.hit(f"register:{client_ip(request)}", 10, 3600)
    email, name = norm_email(body.email), display_name(body.display_name, s)
    password_ok(body.password)
    try:
        uid = db.execute("INSERT INTO users (email, display_name, password, created_at) VALUES (?, ?, ?, ?)",
                         (email, name, hashed(body.password), now())).lastrowid
    except sqlite3.IntegrityError:
        raise HTTPException(409, "There is already an account with this email: sign in, or reset its password.")
    u = fresh(db, uid)
    start_session(db, uid, response, request)
    send_verify(db, u, tasks, s)
    return me_json(u, s)


@r.post("/auth/login")
def login(body: Login, request: Request, response: Response, db=DB):
    s, lim = request.app.state.settings, request.app.state.limiter
    email = str(body.email).strip().lower()[:254]
    ip = client_ip(request)
    lim.hit(f"login-ip:{ip}", 20, 900)
    fails = (f"login-fail:{email}:{ip}", 10, 900), (f"login-fail:{email}", 50, 3600)   # (only failures count: no
    for f in fails:                                                                  # one locks another out)
        lim.hit(*f, count=False)
    u = db.execute("SELECT * FROM users WHERE email = ?", (email,)).fetchone()
    if not check_password(u["password"] if u else None, body.password):
        for f in fails:
            lim.hit(*f)
        raise HTTPException(401, "Wrong email or password.")
    if u["banned_at"]:
        raise HTTPException(403, BANNED)
    if ph.check_needs_rehash(u["password"]):
        db.execute("UPDATE users SET password = ? WHERE id = ?", (hashed(body.password), u["id"]))
    start_session(db, u["id"], response, request)
    return me_json(u, s)


@r.post("/auth/logout")
def logout(request: Request, response: Response, body: Logout | None = None, db=DB):
    u = session_of(request, db)
    if u and body and body.all:
        db.execute("DELETE FROM sessions WHERE user_id = ?", (u["id"],))
    elif u:
        db.execute("DELETE FROM sessions WHERE hash = ?", (u["hash"],))
    response.delete_cookie(COOKIE, path="/api", secure=request.app.state.settings.cookie_secure, httponly=True,
                           samesite="lax")
    return {"ok": True}


@r.post("/auth/verify")
def verify(body: Token, request: Request, u=Depends(user), db=DB):
    """signed in as the account the link was sent for (or whoever opened it would verify someone else's account,
    made with their email, for its maker)"""
    request.app.state.limiter.hit(f"token:{client_ip(request)}", 30, 3600)
    uid = use_token(db, body.token, "verify", u["id"])
    if not uid:
        raise HTTPException(400, "This link doesn't work any more, or it is another account's: sign in with the "
                                 "account it was sent for, or ask for a new one.")
    db.execute("UPDATE users SET verified_at = COALESCE(verified_at, ?) WHERE id = ?", (now(), uid))
    db.execute("DELETE FROM tokens WHERE user_id = ? AND kind = 'verify'", (uid,))
    return {"ok": True, "verified": True}


@r.post("/auth/resend")
def resend(request: Request, tasks: BackgroundTasks, u=Depends(user), db=DB):
    if u["verified_at"]:
        return {"ok": True, "verified": True}
    request.app.state.limiter.hit(f"resend:{u['id']}", 3, 3600)
    send_verify(db, u, tasks, request.app.state.settings)
    return {"ok": True, "verified": False}


@r.post("/auth/forgot")
def forgot(body: Email, request: Request, tasks: BackgroundTasks, db=DB):
    """always the same answer: whether there is such an account is not said"""
    s, lim = request.app.state.settings, request.app.state.limiter
    email = str(body.email).strip().lower()[:254]
    lim.hit(f"forgot-ip:{client_ip(request)}", 5, 3600)
    lim.hit(f"forgot-email:{email}", 3, 3600)
    u = db.execute("SELECT * FROM users WHERE email = ? AND banned_at IS NULL", (email,)).fetchone()
    if u:
        db.execute("DELETE FROM tokens WHERE user_id = ? AND kind = 'reset'", (u["id"],))
        t = make_token(db, u["id"], "reset", RESET_H)
        tasks.add_task(mail.reset, s, u["email"], f"{s.url}/#reset={t}")
    return {"ok": True}


@r.post("/auth/reset")
def reset(body: Reset, request: Request, response: Response, db=DB):
    """a new password: every other session ends; the email is verified (the link came to it); signed in"""
    s = request.app.state.settings
    request.app.state.limiter.hit(f"token:{client_ip(request)}", 30, 3600)
    password_ok(body.password)
    uid = use_token(db, body.token, "reset")
    if not uid:
        raise HTTPException(400, "This link doesn't work any more: ask for a new one.")
    if fresh(db, uid)["banned_at"]:
        raise HTTPException(403, BANNED)
    with tx(db):
        db.execute("UPDATE users SET password = ?, verified_at = COALESCE(verified_at, ?) WHERE id = ?",
                   (hashed(body.password), now(), uid))
        db.execute("DELETE FROM sessions WHERE user_id = ?", (uid,))
    start_session(db, uid, response, request)
    return me_json(fresh(db, uid), s)


# ---------------------------------------------------------------------------------------------- Google
def b64url(b):
    return base64.urlsafe_b64encode(b).rstrip(b"=").decode()


def claims_of(id_token):
    """the ID token's claims. It came straight from Google's token endpoint over TLS, so its signature need not be
    checked (OpenID Connect Core 3.1.3.7); its issuer, audience, expiry and nonce are"""
    try:
        part = id_token.split(".")[1]
        c = json.loads(base64.urlsafe_b64decode(part + "=" * (-len(part) % 4)))
        return c if isinstance(c, dict) else {}
    except (IndexError, ValueError, AttributeError):
        return {}


def google_email(c, client_id, nonce):
    """the email the ID token's claims vouch for, or None"""
    try:
        email = str(c.get("email", "")).strip().lower()
        ok = (c.get("iss") in ("https://accounts.google.com", "accounts.google.com") and c.get("aud") == client_id
              and int(c.get("exp", 0)) >= now() and c.get("nonce") == nonce and c.get("email_verified") is True)
        return email if ok and EMAIL.match(email) and len(email) <= 254 else None
    except (TypeError, ValueError):
        return None


@r.get("/auth/providers")
def providers(request: Request):
    """the ways to sign in, for the page (Continue with Google only when it is set up)"""
    return {"password": True, "google": bool(request.app.state.settings.google_id)}


@r.get("/auth/session")
def session(request: Request, u=Depends(maybe_user)):
    """the page's first question, never a 401: who this browser is (null: signed out), the ways to sign in"""
    s = request.app.state.settings
    return {"me": me_json(u, s) if u else None, "password": True, "google": bool(s.google_id)}


@r.get("/auth/google/start")
def google_start(request: Request, next: str = "/", db=DB):
    s = request.app.state.settings
    if not s.google_id:
        raise HTTPException(404, "Google sign-in isn't set up.")
    request.app.state.limiter.hit(f"google:{client_ip(request)}", 30, 3600)
    state, verifier, nonce = secrets.token_urlsafe(32), secrets.token_urlsafe(48), secrets.token_urlsafe(16)
    if not next.startswith("/") or next.startswith("//") or "\\" in next or len(next) > 200:
        next = "/"
    db.execute("INSERT INTO oauth (hash, verifier, nonce, next, expires_at) VALUES (?, ?, ?, ?, ?)",
               (sha(state), verifier, nonce, next, now() + 600))
    q = {"client_id": s.google_id, "redirect_uri": s.google_redirect, "response_type": "code",
         "scope": "openid email profile", "state": state, "nonce": nonce, "prompt": "select_account",
         "code_challenge": b64url(hashlib.sha256(verifier.encode()).digest()), "code_challenge_method": "S256"}
    out = RedirectResponse(f"{s.google_auth_url}?{urlencode(q)}", 302)
    out.set_cookie(OAUTH_COOKIE, state, max_age=600, path="/api/auth/google", secure=s.cookie_secure, httponly=True,
                   samesite="lax")
    return out


@r.get("/auth/google/callback")
def google_callback(request: Request, code: str = "", state: str = "", error: str = "", db=DB):
    """back from Google: the code for tokens, the account found by its email (Google's verified) or made; then the
    page (#google=failed or #google=banned when it didn't work)"""
    s = request.app.state.settings
    request.app.state.limiter.hit(f"google:{client_ip(request)}", 30, 3600)

    def back(to):
        out = RedirectResponse(to, 303)
        out.delete_cookie(OAUTH_COOKIE, path="/api/auth/google", secure=s.cookie_secure, httponly=True, samesite="lax")
        return out

    mine, rows = request.cookies.get(OAUTH_COOKIE, ""), []
    if state and mine and secrets.compare_digest(state.encode(), mine.encode()):    # (this browser's sign-in)
        rows = db.execute("DELETE FROM oauth WHERE hash = ? AND expires_at > ? RETURNING verifier, nonce, next",
                          (sha(state), now())).fetchall()
    if error or not code or not rows:
        return back("/#google=failed")
    verifier, nonce, nxt = rows[0]
    try:
        t = request.app.state.http.post(s.google_token_url, data={
            "code": code, "client_id": s.google_id, "client_secret": s.google_secret,
            "redirect_uri": s.google_redirect, "grant_type": "authorization_code", "code_verifier": verifier})
        c = claims_of(t.json()["id_token"]) if t.status_code == 200 else {}
    except (httpx.HTTPError, ValueError, KeyError, TypeError):
        c = {}
    email = google_email(c, s.google_id, nonce)
    if not email:
        return back("/#google=failed")
    u = db.execute("SELECT * FROM users WHERE email = ?", (email,)).fetchone()
    if u and u["banned_at"]:
        return back("/#google=banned")
    with tx(db):
        if not u:
            name = clean(str(c.get("name") or c.get("given_name") or "")[:200], 200, "")[:40].strip()
            if not name or filtered(s.words or WORDS, name):
                name = "River listener"
            uid = db.execute("INSERT INTO users (email, display_name, created_at, verified_at) VALUES (?, ?, ?, ?)",
                             (email, name, now(), now())).lastrowid
        else:
            uid = u["id"]
            if not u["verified_at"]:              # someone made it with this email and never proved it: not theirs
                db.execute("UPDATE users SET password = NULL, verified_at = ? WHERE id = ?", (now(), uid))
                db.execute("DELETE FROM sessions WHERE user_id = ?", (uid,))
    out = back(nxt)
    start_session(db, uid, out, request)
    return out


# ---------------------------------------------------------------------------------------------- the account
class MeEdit(BaseModel):
    display_name: str | None = None
    password: str | None = None                   # (the current one, to change it)
    new_password: str | None = None


@r.get("/me")
def me(request: Request, u=Depends(user)):
    return me_json(u, request.app.state.settings)


@r.patch("/me")
def me_edit(body: MeEdit, request: Request, u=Depends(user), db=DB):
    s = request.app.state.settings
    if body.display_name is not None:
        db.execute("UPDATE users SET display_name = ? WHERE id = ?", (display_name(body.display_name, s), u["id"]))
    if body.new_password is not None:
        password_ok(body.new_password)
        request.app.state.limiter.hit(f"password:{u['id']}", 10, 3600)
        if u["password"] and not check_password(u["password"], body.password or ""):
            raise HTTPException(403, "Your current password isn't that one.")
        with tx(db):
            db.execute("UPDATE users SET password = ? WHERE id = ?", (hashed(body.new_password), u["id"]))
            db.execute("DELETE FROM sessions WHERE user_id = ? AND hash != ?", (u["id"], u["hash"]))
    return me_json(fresh(db, u["id"]), s)


@r.delete("/me")
def me_delete(request: Request, response: Response, u=Depends(user), db=DB):
    """everything: the account, its sessions, shards, backups, reports (the copies others saved of its shards stay
    theirs, without its name: CC BY 4.0 3(a)(3))"""
    db.execute("DELETE FROM users WHERE id = ?", (u["id"],))
    db.execute("DELETE FROM hits WHERE who = ?", (f"u{u['id']}",))
    response.delete_cookie(COOKIE, path="/api", secure=request.app.state.settings.cookie_secure, httponly=True,
                           samesite="lax")
    return {"ok": True}
