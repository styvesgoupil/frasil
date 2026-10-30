# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""The admin page's (admin.frasilsynth.com, website/admin/): the River's accounts, each one's uploads, the waitlists.
Read-only. Every request carries X-Frasil-Admin, FRASIL_ADMIN_KEY's value (Caddy adds it, after its own basic auth,
on admin.frasilsynth.com, and keeps /api/admin/ off the River's domain); without the key set, or with any other
header, these routes are 404, as a path that isn't there. The key is never logged nor answered.

(The moderators' routes, /api/admin/reports, .../hide, .../ban: shards.py, a River admin's session.)"""
import hmac

from fastapi import APIRouter, Depends, HTTPException, Request

from auth import DB, is_admin
from shards import FAVS, Id
from store import iso
from waitlist import LISTS

HEADER = "x-frasil-admin"


def keyed(request: Request):
    k = request.app.state.settings.admin_key
    if not k or not hmac.compare_digest(request.headers.get(HEADER, "").encode(), k.encode()):
        raise HTTPException(404, "Not Found")     # (FastAPI's own 404: nothing says something is here)


r = APIRouter(prefix="/api/admin", dependencies=[Depends(keyed)])
COUNT = "(SELECT COUNT(*) FROM shards s WHERE s.owner_id = u.id{})"
USERS = ("SELECT u.*, " + COUNT.format(" AND s.visibility = 'public'") + " AS n_public, "
         + COUNT.format(" AND s.visibility = 'private'") + " AS n_private, "
         + COUNT.format(" AND s.visibility = 'hidden'") + " AS n_hidden, "
         "(SELECT COUNT(*) FROM backups b WHERE b.owner_id = u.id) AS n_backups, "
         "(SELECT COUNT(*) FROM favorites f WHERE f.user_id = u.id) AS n_favorites FROM users u")


def account_json(u, s):
    """sign_in: password (Google may sign it in too: that isn't kept) or google (made, or claimed, with Google)"""
    return {"id": u["id"], "display_name": u["display_name"], "email": u["email"], "created_at": iso(u["created_at"]),
            "verified": bool(u["verified_at"]), "verified_at": iso(u["verified_at"]),
            "sign_in": "password" if u["password"] else "google", "banned": bool(u["banned_at"]),
            "admin": is_admin(u, s),
            "shards": {"public": u["n_public"], "private": u["n_private"], "hidden": u["n_hidden"],
                       "total": u["n_public"] + u["n_private"] + u["n_hidden"]},
            "backups": u["n_backups"], "favorites": u["n_favorites"]}


def counts(db):
    return {k: db.execute("SELECT COUNT(*) FROM waitlist WHERE list = ?", (k,)).fetchone()[0] for k in LISTS}


@r.get("/overview")
def overview(request: Request, db=DB):
    """every account, the newest first; the totals; how many on each waitlist"""
    s = request.app.state.settings
    users = [account_json(u, s) for u in db.execute(USERS + " ORDER BY u.created_at DESC, u.id DESC")]
    one = lambda q: db.execute(q).fetchone()[0]
    return {"users": users,
            "totals": {"users": len(users), "verified": sum(u["verified"] for u in users),
                       "shards": one("SELECT COUNT(*) FROM shards"),
                       "public": one("SELECT COUNT(*) FROM shards WHERE visibility = 'public'"),
                       "backups": one("SELECT COUNT(*) FROM backups")},
            "waitlist": counts(db)}


@r.get("/users/{uid}")
def account(uid: Id, request: Request, db=DB):
    """one account, its shards and its backups, the newest first (no shard's data, no backup's image)"""
    u = db.execute(USERS + " WHERE u.id = ?", (uid,)).fetchone()
    if not u:
        raise HTTPException(404, "No such user.")
    shards = db.execute(f"SELECT s.id, s.name, s.description, s.layout, s.firmware, s.visibility, s.created_at, "
                        f"s.updated_at, s.shared_at, s.plays, s.sends, s.saves, {FAVS} AS favorites, s.origin_id, "
                        "o.display_name AS origin_author FROM shards s LEFT JOIN users o ON o.id = s.origin_author_id "
                        "WHERE s.owner_id = ? ORDER BY s.created_at DESC, s.id", (uid,))
    backups = db.execute("SELECT id, created_at, note, firmware, length(image) AS size FROM backups "
                         "WHERE owner_id = ? ORDER BY created_at DESC, id", (uid,))
    return {"user": account_json(u, request.app.state.settings),
            "shards": [{"id": x["id"], "name": x["name"], "description": x["description"], "layout": x["layout"],
                        "firmware": x["firmware"], "visibility": x["visibility"], "public": x["visibility"] == "public",
                        "created_at": iso(x["created_at"]), "updated_at": iso(x["updated_at"]),
                        "shared_at": iso(x["shared_at"]), "plays": x["plays"], "sends": x["sends"],
                        "saves": x["saves"], "favorites": x["favorites"], "origin_id": x["origin_id"],
                        "origin_author": x["origin_author"]} for x in shards],
            "backups": [{"id": b["id"], "created_at": iso(b["created_at"]), "note": b["note"],
                         "firmware": b["firmware"], "size": b["size"]} for b in backups]}


@r.get("/waitlist")
def waitlists(db=DB):
    """{lists: {ipad: {count, items: [{email, created_at}]}, vst: ...}}, the newest first"""
    out = {}
    for k in LISTS:
        rows = db.execute("SELECT email, created FROM waitlist WHERE list = ? ORDER BY created DESC, id DESC", (k,))
        items = [{"email": x["email"], "created_at": iso(x["created"])} for x in rows]
        out[k] = {"count": len(items), "items": items}
    return {"lists": out}
