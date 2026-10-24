# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""What keeps the River safe: the rate limits (in this process's memory: one uvicorn worker), the client's address
(hashed before it is kept), the word filter, the requests that change something (same origin, JSON), the text
users type (cleaned)."""
import hashlib
import re
import threading
import time
import unicodedata
from collections import deque

from fastapi import HTTPException, Request
from fastapi.responses import JSONResponse

# the word filter's default (WORD_FILTER replaces it): these words (and their -s, -ed, -er, -ing) are refused in what
# is shown publicly: names, titles, descriptions, tags, display names. Whole words only ("spice" is no "spic")
WORDS = ["fuck", "motherfucker", "shit", "bullshit", "cunt", "nigger", "nigga", "faggot", "kike", "spic", "tranny",
         "whore", "slut", "porn", "hentai", "nazi", "hitler", "rape", "rapist", "viagra", "casino", "onlyfans"]
CHANGES = {"POST", "PUT", "PATCH", "DELETE"}
BODY_MAX = 64 * 1024


class Limiter:
    """n hits per `per` seconds for a key ("login-ip:1.2.3.4"); the oldest forgotten as they age"""

    def __init__(self):
        self.hits, self.lock, self.count = {}, threading.Lock(), 0

    def hit(self, key, n, per, count=True):
        """count=False: only whether the key has its n already (429 then)"""
        t = time.monotonic()
        with self.lock:
            q = self.hits.setdefault(key, deque())
            while q and q[0] <= t - per:
                q.popleft()
            if len(q) >= n:
                raise HTTPException(429, "Too many tries. Wait a little, then try again.",
                                    headers={"Retry-After": str(int(q[0] + per - t) + 1)})
            if not count:
                return
            q.append(t)
            self.count += 1
            if self.count % 1000 == 0:              # (now and then, the keys with nothing left)
                for k in [k for k, v in self.hits.items() if not v or v[-1] <= t - 86400]:
                    del self.hits[k]


def client_ip(request: Request):
    h = request.app.state.settings.ip_header
    ip = (request.headers.get(h, "") if h else "") or request.headers.get("x-forwarded-for", "").split(",")[0]
    return ip.strip() or (request.client.host if request.client else "?")


def ip_hash(request: Request):
    """the client's address as kept (reports, counters): salted (the database's own salt), cut short"""
    return hashlib.sha256((request.app.state.salt + client_ip(request)).encode()).hexdigest()[:24]


def filtered(words, *texts):
    """the first word refused in texts, or None"""
    pat = words and re.compile(r"\b(" + "|".join(re.escape(w.lower()) for w in words) + r")(?:s|es|ed|er|ers|ing)?\b")
    for t in texts:
        m = pat and t and pat.search(unicodedata.normalize("NFKD", str(t)).lower())
        if m:
            return m.group(1)
    return None


def clean(text, n, what, lines=False):
    """what a user typed: no control characters (newlines kept in lines), spaces trimmed; n characters at most"""
    t = unicodedata.normalize("NFC", str(text or ""))
    t = "".join(c for c in t if (lines and c == "\n") or not unicodedata.category(c).startswith("C"))
    t = re.sub(r"\n{3,}", "\n\n", t) if lines else re.sub(r"\s+", " ", t)
    t = t.strip()
    if len(t) > n:
        raise HTTPException(422, f"{what}: {n} characters at most.")
    return t


async def same_origin(request: Request, call_next):
    """a request that changes something: from the site itself (Origin), as JSON (no form can send it from elsewhere
    without the browser asking first), not too big"""
    if request.method in CHANGES and request.url.path.startswith("/api/"):
        if request.headers.get("origin", "") != request.app.state.origin:
            return JSONResponse({"detail": "This request didn't come from the River."}, 403)
        ct = request.headers.get("content-type", "").split(";")[0].strip().lower()
        if request.method != "DELETE" and ct != "application/json":
            return JSONResponse({"detail": "Send JSON (Content-Type: application/json)."}, 415)
        n = request.headers.get("content-length", "0")
        if not n.isdigit() or int(n) > BODY_MAX:
            return JSONResponse({"detail": "Too big."}, 413)
    response = await call_next(request)
    if request.url.path.startswith("/api/"):
        response.headers.setdefault("Cache-Control", "no-store")
        response.headers.setdefault("X-Content-Type-Options", "nosniff")
    return response
