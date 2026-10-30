# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""The waitlists: who wants to hear when FRASIL for iPad or FRASIL VST is out. An email, kept lowercased, once per
list; no email sent, none verified (the sites' Coming soon buttons: website/assets/js/site.js). The main site
(frasilsynth.com) sends its /api/waitlist here too, so its Origin is accepted for this one (guard.same_origin)."""
import re
import unicodedata

from fastapi import APIRouter, HTTPException, Request
from pydantic import BaseModel

from auth import DB
from guard import client_ip
from store import now

r = APIRouter(prefix="/api")
LISTS = ("ipad", "vst")
NOT = r'@\s<>,;"()\[\]\\'                        # (none of these in an address)
SHAPE = re.compile(rf"^[^{NOT}]+@[^{NOT}.]+(\.[^{NOT}.]+)+$")    # one @, then a dot, nothing empty around them


class Signup(BaseModel):
    list: str
    email: str


def email_of(text):
    """the address as kept (trimmed, lowercased), or a 422: 254 characters at most, one @, a dot after it"""
    e = str(text or "").strip().lower()
    if not 3 <= len(e) <= 254 or not SHAPE.match(e) or any(unicodedata.category(c).startswith("C") for c in e):
        raise HTTPException(422, "Enter a valid email address.")
    return e


@r.post("/waitlist")
def join(body: Signup, request: Request, db=DB):
    """{list: ipad | vst, email} -> {ok: true}; signed up already: the same (nothing added)"""
    request.app.state.limiter.hit(f"waitlist:{client_ip(request)}", 20, 3600)
    if body.list not in LISTS:
        raise HTTPException(422, "list: ipad or vst.")
    db.execute("INSERT OR IGNORE INTO waitlist (list, email, created) VALUES (?, ?, ?)",
               (body.list, email_of(body.email), now()))
    return {"ok": True}
