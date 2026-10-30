# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""The River's shards (a scene of FRASIL: its scene_t, as the FM-1 keeps it) and backups (an fm1-base image, as the
FM-1 makes it): each user's library and favorites (bookmarks to others' shards), the public gallery (new, played,
favorites, popular; filters; 24 a page), what is counted (plays, sends: once a day per listener), the reports and the
admins' moderation, the account's export. A shard has a name (the FM-1's) and a description: no title, no tags (River
v2; what v1's page still sends of them is ignored)."""
import base64
import binascii
import io
import json
import re
import zipfile
from datetime import date
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, Path, Query, Request, Response
from pydantic import BaseModel

from auth import DB, admin, is_admin, maybe_user, me_json, user
from guard import clean, client_ip, filtered, ip_hash, WORDS
from scenes import Invalid
from store import iso, new_id, now, tx

r = APIRouter(prefix="/api")
PAGE, LICENSE = 24, "CC BY 4.0"
MAX_SHARDS, MAX_BACKUPS = 2000, 200               # (an account's)
FIRMWARE = re.compile(r"^[0-9A-Za-z.+-]{0,16}$")
Id = Annotated[int, Path(ge=1, le=2 ** 62)]           # (a number SQLite can hold)
FAVS = "(SELECT COUNT(*) FROM favorites f WHERE f.shard_id = s.id)"
KEPT = ("s.saves + (SELECT COUNT(*) FROM favorites f WHERE f.shard_id = s.id AND NOT EXISTS (SELECT 1 FROM saved v "
        "WHERE v.shard_id = s.id AND v.user_id = f.user_id))")         # (store.popular's: who kept it, each once)
MINE = (f"SELECT s.*, o.display_name AS origin_author, {FAVS} AS favorites FROM shards s "
        "LEFT JOIN users o ON o.id = s.origin_author_id WHERE s.owner_id = ?")
PUBLIC = ("FROM shards s JOIN users u ON u.id = s.owner_id LEFT JOIN users o ON o.id = s.origin_author_id "
          "WHERE s.visibility = 'public' AND u.banned_at IS NULL")
PUBLIC_ROW = (f"SELECT s.*, u.display_name AS author, o.display_name AS origin_author, {FAVS} AS favorites, "
              "EXISTS (SELECT 1 FROM favorites f WHERE f.shard_id = s.id AND f.user_id = ?) AS favorited " + PUBLIC)
FAVORITES = (f"SELECT s.*, u.display_name AS author, o.display_name AS origin_author, {FAVS} AS favorites, "
             "1 AS favorited, fv.created_at AS favorited_at FROM favorites fv JOIN shards s ON s.id = fv.shard_id "
             "JOIN users u ON u.id = s.owner_id LEFT JOIN users o ON o.id = s.origin_author_id "
             "WHERE fv.user_id = ? AND s.visibility != 'hidden' AND u.banned_at IS NULL")
SORTS = {"new": "s.shared_at DESC, s.id", "played": "s.plays DESC, s.shared_at DESC, s.id",
         "favorites": "favorites DESC, s.shared_at DESC, s.id",
         "popular": f"popular(s.plays, s.sends, {KEPT}, s.shared_at, ?) DESC, s.shared_at DESC, s.id"}
BASS = {"seq": 1, "none": 0}                      # (the gallery's bass: its sequence on, or off)


def shard_json(x):
    return {"id": x["id"], "name": x["name"], "description": x["description"], "layout": x["layout"],
            "firmware": x["firmware"], "data": base64.b64encode(x["data"]).decode(), "from_slot": x["from_slot"],
            "root": x["root"], "scale": x["scale"], "type": x["type"], "drone": x["drone"], "glint": x["glint"],
            "bseq": x["bseq"], "visibility": x["visibility"], "license": x["license"], "origin_id": x["origin_id"],
            "origin_author": x["origin_author"], "created_at": iso(x["created_at"]),
            "updated_at": iso(x["updated_at"]), "shared_at": iso(x["shared_at"]), "plays": x["plays"],
            "sends": x["sends"], "saves": x["saves"], "favorites": x["favorites"]}


def public_json(x):
    """favorited: among the signed-in user's favorites (false: not, or signed out)"""
    j = shard_json(x)
    for k in ("from_slot", "visibility", "updated_at"):
        del j[k]
    return {**j, "author": x["author"], "favorited": bool(x["favorited"])}


def favorite_json(x):
    """shared: false when its author took it out of the River since (it plays, and goes to the FM-1, all the same)"""
    return {**public_json(x), "favorited_at": iso(x["favorited_at"]), "shared": x["visibility"] == "public"}


def unb64(text, what):
    try:
        return base64.b64decode(str(text or ""), validate=True)
    except (binascii.Error, ValueError):
        raise HTTPException(422, f"{what}: not base64.")


def checked(fn, *a):
    try:
        return fn(*a)
    except Invalid as e:
        raise HTTPException(422, str(e))


def shown(request, *texts):
    """what goes public passes the word filter"""
    w = filtered(request.app.state.settings.words or WORDS, *texts)
    if w:
        raise HTTPException(422, f'The River doesn\'t show "{w}". Change it, then try again.')


def mine(db, sid, u):
    x = db.execute(MINE + " AND s.id = ?", (u["id"], sid)).fetchone()
    if not x:
        raise HTTPException(404, "No such shard in your library.")
    return x


def public(db, sid, uid=None):
    x = db.execute(PUBLIC_ROW + " AND s.id = ?", (uid, sid)).fetchone()
    if not x:
        raise HTTPException(404, "No such shard in the River.")
    return x


# ---------------------------------------------------------------------------------------------- my library
class ShardIn(BaseModel):                         # (a field not here is ignored: v1's page's title and tags)
    name: str | None = None                       # (given: written into data's name)
    layout: str
    firmware: str = ""
    data: str                                     # the scene_t, base64
    from_slot: int | None = None                  # the FM-1's slot it came from, 0-15 (the page's scene.slot)
    description: str = ""
    origin: str | None = None                     # the shard it was made from (the editor's SAVE AS): someone else's
                                                  # public one is credited (CC BY 4.0), one's own passes its credit on


class ShardEdit(BaseModel):
    name: str | None = None
    description: str | None = None
    data: str | None = None                       # the editor's SAVE: its sound changed (the scene_t, base64, in the
    layout: str | None = None                     # current layout; its name stays the shard's, or the edit's)
    firmware: str | None = None


def edited(request, x, e):
    """a shard's fields with an edit's applied -> (data, name, description, bseq)"""
    sch = request.app.state.schema
    data = x["data"]
    if e.data is not None:                        # (its sound: as checked as a new shard's, under its own name)
        if e.layout != sch.layout:
            raise HTTPException(422, f"layout: {sch.layout} only (update the page).")
        data = checked(sch.renamed, checked(sch.shard, unb64(e.data, "data"), sch.layout), sch.name_of(x["data"]))
    data = checked(sch.renamed, data, e.name.strip().upper()) if e.name is not None else data
    return (data, sch.name_of(data),
            x["description"] if e.description is None else clean(e.description, 1000, "A description", lines=True),
            sch.derived(data)["bseq"] if e.data is not None or x["layout"] == sch.layout else x["bseq"])


def rewrite(db, sid, data, name, description, bseq, *more):
    cols = ", ".join(f"{k} = ?" for k in ("data", "name", "description", "bseq", "updated_at"))
    db.execute(f"UPDATE shards SET {cols}{''.join(', ' + m for m in more)} WHERE id = ?",
               (data, name, description, bseq, now(), sid))


@r.get("/shards")
def shards(u=Depends(user), db=DB):
    return {"items": [shard_json(x) for x in db.execute(MINE + " ORDER BY s.created_at DESC, s.id", (u["id"],))]}


@r.post("/shards", status_code=201)
def shard_add(body: ShardIn, request: Request, u=Depends(user), db=DB):
    sch = request.app.state.schema
    origin = credit = None
    if body.origin:                               # (made from another: its first author credited, as a copy's)
        o = db.execute("SELECT origin_id, origin_author_id FROM shards WHERE id = ? AND owner_id = ?",
                       (body.origin, u["id"])).fetchone()
        if o:                                     # one's own: its credit passed on, if it had one
            origin, credit = (o["origin_id"], o["origin_author_id"]) if o["origin_id"] else (None, None)
        else:                                     # someone else's in the River, or among one's favorites (shared
            x = db.execute(FAVORITES + " AND s.id = ?", (u["id"], body.origin)).fetchone()   # no longer, it still is)
            x = x or public(db, body.origin)
            origin, credit = body.origin, x["origin_author_id"] if x["origin_id"] else x["owner_id"]
    request.app.state.limiter.hit(f"upload:{u['id']}", 200, 3600)
    raw = unb64(body.data, "data")                # (a page still open from before: its layout made over, as at startup)
    up = sch.upgraded(raw, body.layout) if body.layout != sch.layout else None
    data = checked(sch.shard, up or raw, sch.layout if up else body.layout)
    if body.name is not None and body.name.strip().upper() != sch.name_of(data):
        data = checked(sch.renamed, data, body.name.strip().upper())
    if body.from_slot is not None and not 0 <= body.from_slot < sch.s["sizes"]["NSCENES"]:
        raise HTTPException(422, "from_slot: 0 to 15 (the FM-1's slot, from 0).")
    if not FIRMWARE.match(body.firmware):
        raise HTTPException(422, "firmware: a version, such as 1.1.")
    if db.execute("SELECT COUNT(*) FROM shards WHERE owner_id = ?", (u["id"],)).fetchone()[0] >= MAX_SHARDS:
        raise HTTPException(409, f"Your library is full ({MAX_SHARDS} shards).")
    description, d, t = clean(body.description, 1000, "A description", lines=True), sch.derived(data), now()
    with tx(db):
        sid = new_id(db, "shards")
        db.execute("INSERT INTO shards (id, owner_id, name, description, layout, firmware, data, from_slot, root, "
                   "scale, type, drone, glint, bseq, origin_id, origin_author_id, created_at, updated_at) "
                   "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                   (sid, u["id"], sch.name_of(data), description, sch.layout, body.firmware, data, body.from_slot,
                    d["root"], d["scale"], d["type"], d["drone"], d["glint"], d["bseq"], origin, credit, t, t))
    return shard_json(mine(db, sid, u))


@r.patch("/shards/{sid}")
def shard_edit(sid: str, body: ShardEdit, request: Request, u=Depends(user), db=DB):
    x = mine(db, sid, u)
    if body.data is not None and body.firmware is not None and not FIRMWARE.match(body.firmware):
        raise HTTPException(422, "firmware: a version, such as 1.1.")
    f = edited(request, x, body)
    if x["visibility"] == "public":
        shown(request, *f[1:3])
    with tx(db):
        rewrite(db, sid, *f)
        if body.data is not None:                 # (what the lists filter by, from its sound; its layout the current)
            sch = request.app.state.schema
            d = sch.derived(f[0])
            db.execute("UPDATE shards SET layout = ?, root = ?, scale = ?, type = ?, drone = ?, glint = ?, firmware = ? "
                       "WHERE id = ?", (sch.layout, d["root"], d["scale"], d["type"], d["drone"], d["glint"],
                                        body.firmware or x["firmware"], sid))
    return shard_json(mine(db, sid, u))


@r.delete("/shards/{sid}")
def shard_delete(sid: str, u=Depends(user), db=DB):
    mine(db, sid, u)
    db.execute("DELETE FROM shards WHERE id = ?", (sid,))
    return {"ok": True}


@r.post("/shards/{sid}/share")
def share(sid: str, request: Request, body: ShardEdit | None = None, u=Depends(user), db=DB):
    """into the public River, under CC BY 4.0 (its author named); an edit may come with it (a name, a description)"""
    x = mine(db, sid, u)
    if body and body.data is not None:
        raise HTTPException(422, "data: not here (PATCH the shard first).")
    if not u["verified_at"]:
        raise HTTPException(403, "Confirm your email first: the River sent you a link (or ask for a new one).")
    if x["visibility"] == "hidden":
        raise HTTPException(403, "The River's moderators hid this shard.")
    f = edited(request, x, body or ShardEdit())
    shown(request, *f[1:3])
    if x["visibility"] != "public":
        request.app.state.limiter.hit(f"share:{u['id']}", 50, 86400)
    rewrite(db, sid, *f, "visibility = 'public'", f"license = '{LICENSE}'",
            f"shared_at = COALESCE(shared_at, {now()})")
    return shard_json(mine(db, sid, u))


@r.post("/shards/{sid}/unshare")
def unshare(sid: str, u=Depends(user), db=DB):
    mine(db, sid, u)
    db.execute("UPDATE shards SET visibility = 'private', updated_at = ? WHERE id = ? AND visibility = 'public'",
               (now(), sid))
    return shard_json(mine(db, sid, u))


# ---------------------------------------------------------------------------------------------- my backups
class BackupIn(BaseModel):
    image: str                                    # the .frasil file, base64
    note: str = ""
    firmware: str = ""


def backup_json(request, b, image=False):
    """scenes: the names saved in it, slot by slot (null: nothing saved there; null for all: an earlier FRASIL's)"""
    j = {"id": b["id"], "created_at": iso(b["created_at"]), "note": b["note"], "firmware": b["firmware"],
         "size": len(b["image"]), "scenes": request.app.state.schema.backup(b["image"])}
    return {**j, "image": base64.b64encode(b["image"]).decode()} if image else j


def my_backup(db, bid, u):
    b = db.execute("SELECT * FROM backups WHERE id = ? AND owner_id = ?", (bid, u["id"])).fetchone()
    if not b:
        raise HTTPException(404, "No such backup.")
    return b


@r.get("/backups")
def backups(request: Request, u=Depends(user), db=DB):
    rows = db.execute("SELECT * FROM backups WHERE owner_id = ? ORDER BY created_at DESC, id", (u["id"],))
    return {"items": [backup_json(request, b) for b in rows]}


@r.post("/backups", status_code=201)
def backup_add(body: BackupIn, request: Request, u=Depends(user), db=DB):
    request.app.state.limiter.hit(f"backup:{u['id']}", 30, 3600)
    image = unb64(body.image, "image")
    checked(request.app.state.schema.backup, image)
    if not FIRMWARE.match(body.firmware):
        raise HTTPException(422, "firmware: a version, such as 1.1.")
    if db.execute("SELECT COUNT(*) FROM backups WHERE owner_id = ?", (u["id"],)).fetchone()[0] >= MAX_BACKUPS:
        raise HTTPException(409, f"You have {MAX_BACKUPS} backups: delete some first.")
    with tx(db):
        bid = new_id(db, "backups")
        db.execute("INSERT INTO backups (id, owner_id, created_at, note, firmware, image) VALUES (?, ?, ?, ?, ?, ?)",
                   (bid, u["id"], now(), clean(body.note, 200, "A note"), body.firmware, image))
    return backup_json(request, my_backup(db, bid, u))


@r.get("/backups/{bid}")
def backup_get(bid: str, request: Request, u=Depends(user), db=DB):
    return backup_json(request, my_backup(db, bid, u), image=True)


@r.delete("/backups/{bid}")
def backup_delete(bid: str, u=Depends(user), db=DB):
    my_backup(db, bid, u)
    db.execute("DELETE FROM backups WHERE id = ?", (bid,))
    return {"ok": True}


# ---------------------------------------------------------------------------------------------- the public River
@r.get("/public/shards")
def gallery(request: Request, sort: str = "new", q: str = "", root: str = "", scale: str = "", type: str = "",
            drone: str = "", bass: str = "", page: int = Query(1, ge=1, le=100000), u=Depends(maybe_user), db=DB):
    """sort: new (the newest shared first), played (the most played), favorites (the most favorited), popular (the
    charts: store.popular); root, scale, type (GLINT's OFF, ARP, SEQ), drone (its preset): a word as the FM-1 shows
    it, or its number; bass: seq (its sequence on) or none; q: words, each in the name or the description"""
    sch = request.app.state.schema
    if sort not in SORTS or len(q) > 100:
        raise HTTPException(422, "sort: new, played, favorites or popular; q: 100 characters at most.")
    where, args = [], []
    for k, v in (("root", root), ("scale", scale), ("type", type), ("drone", drone)):
        if v.strip():
            n = sch.value(k, v)
            if n is None:
                raise HTTPException(422, f"{k}: no such value ({v}).")
            where.append(f"s.{k} = ?")
            args.append(n)
    if bass.strip():
        if bass.strip().lower() not in BASS:
            raise HTTPException(422, f"bass: seq or none, not {bass}.")
        where.append("s.bseq = ?")
        args.append(BASS[bass.strip().lower()])
    for w in q.split()[:8]:
        like = "%" + w.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_") + "%"
        where.append("(s.name LIKE ? ESCAPE '\\' OR s.description LIKE ? ESCAPE '\\')")
        args += [like] * 2
    cond = "".join(" AND " + w for w in where)
    total = db.execute(f"SELECT COUNT(*) {PUBLIC}{cond}", args).fetchone()[0]
    pages = max(1, -(-total // PAGE))
    rows = db.execute(f"{PUBLIC_ROW}{cond} ORDER BY {SORTS[sort]} LIMIT ? OFFSET ?",
                      [u and u["id"]] + args + ([now()] if sort == "popular" else []) + [PAGE, (page - 1) * PAGE])
    return {"items": [public_json(x) for x in rows], "page": page, "pages": pages, "total": total, "sort": sort}


@r.get("/public/shards/{sid}")
def gallery_one(sid: str, u=Depends(maybe_user), db=DB):
    return public_json(public(db, sid, u and u["id"]))


def counted(request, db, sid, col, u):
    """once a day per listener (a user, or a hashed IP); the author's own not counted"""
    request.app.state.limiter.hit(f"count:{client_ip(request)}", 300, 3600)
    x = public(db, sid)
    who = f"u{u['id']}" if u else ip_hash(request)
    if not (u and u["id"] == x["owner_id"]) and db.execute(
            "INSERT OR IGNORE INTO hits (shard_id, kind, who, day) VALUES (?, ?, ?, ?)",
            (sid, col, who, now() // 86400)).rowcount:
        db.execute(f"UPDATE shards SET {col} = {col} + 1 WHERE id = ?", (sid,))
    return {col: db.execute(f"SELECT {col} FROM shards WHERE id = ?", (sid,)).fetchone()[0]}


@r.post("/public/shards/{sid}/played")
def played(sid: str, request: Request, u=Depends(maybe_user), db=DB):
    return counted(request, db, sid, "plays", u)


@r.post("/public/shards/{sid}/sent")
def sent(sid: str, request: Request, u=Depends(maybe_user), db=DB):
    return counted(request, db, sid, "sends", u)


@r.post("/public/shards/{sid}/save", status_code=201)
def save(sid: str, request: Request, response: Response, u=Depends(user), db=DB):
    """a copy into my library, its first author credited (CC BY 4.0); saved already: that copy (200). v1's SAVE, kept
    until the River v2 ships (its Favorite, a bookmark, replaces it); the copies made stay their users' own shards"""
    x = public(db, sid)
    if x["owner_id"] == u["id"]:
        raise HTTPException(409, "This shard is yours already.")
    had = db.execute("SELECT id FROM shards WHERE owner_id = ? AND origin_id = ?", (u["id"], sid)).fetchone()
    if had:
        response.status_code = 200
        return shard_json(mine(db, had[0], u))
    request.app.state.limiter.hit(f"save:{u['id']}", 100, 3600)
    if db.execute("SELECT COUNT(*) FROM shards WHERE owner_id = ?", (u["id"],)).fetchone()[0] >= MAX_SHARDS:
        raise HTTPException(409, f"Your library is full ({MAX_SHARDS} shards).")
    credit = x["origin_author_id"] if x["origin_id"] else x["owner_id"]
    with tx(db):
        nid, t = new_id(db, "shards"), now()
        db.execute("INSERT INTO shards (id, owner_id, name, description, layout, firmware, data, root, scale, type, "
                   "drone, glint, bseq, origin_id, origin_author_id, created_at, updated_at) SELECT ?, ?, name, "
                   "description, layout, firmware, data, root, scale, type, drone, glint, bseq, id, ?, ?, ? FROM "
                   "shards WHERE id = ?", (nid, u["id"], credit, t, t, sid))
        if db.execute("INSERT OR IGNORE INTO saved (shard_id, user_id) VALUES (?, ?)", (sid, u["id"])).rowcount:
            db.execute("UPDATE shards SET saves = saves + 1 WHERE id = ?", (sid,))   # (once per user, ever)
    return shard_json(mine(db, nid, u))


# ---------------------------------------------------------------------------------------------- my favorites
@r.get("/favorites")
def favorites(u=Depends(user), db=DB):
    """the newest first, each as the River shows it (with its data: it plays and goes to the FM-1 as my own shards).
    One its author took out of the River stays (shared: false); one deleted is gone (with its favorites); one the
    moderators hid, or whose author is banned, is left out (back if that is undone)"""
    rows = db.execute(FAVORITES + " ORDER BY fv.created_at DESC, fv.rowid DESC", (u["id"],))
    return {"items": [favorite_json(x) for x in rows]}


@r.post("/favorites/{sid}", status_code=201)
def favorite(sid: str, request: Request, response: Response, u=Depends(user), db=DB):
    """a bookmark to a shard of the River (not a copy: what its author makes of it, it follows); one already: 200"""
    x = public(db, sid, u["id"])
    if x["owner_id"] == u["id"]:
        raise HTTPException(409, "This shard is yours already.")
    if x["favorited"]:
        response.status_code = 200
    else:
        request.app.state.limiter.hit(f"favorite:{u['id']}", 200, 3600)
        db.execute("INSERT OR IGNORE INTO favorites (shard_id, user_id, created_at) VALUES (?, ?, ?)",
                   (sid, u["id"], now()))
    return {"favorited": True,
            "favorites": db.execute("SELECT COUNT(*) FROM favorites WHERE shard_id = ?", (sid,)).fetchone()[0]}


@r.delete("/favorites/{sid}")
def unfavorite(sid: str, u=Depends(user), db=DB):
    """shared or not (one no longer shared leaves my favorites too); not among them: nothing to do"""
    db.execute("DELETE FROM favorites WHERE shard_id = ? AND user_id = ?", (sid, u["id"]))
    return {"favorited": False,
            "favorites": db.execute("SELECT COUNT(*) FROM favorites WHERE shard_id = ?", (sid,)).fetchone()[0]}


# ---------------------------------------------------------------------------------------------- reports
class ReportIn(BaseModel):
    reason: str


@r.post("/public/shards/{sid}/report", status_code=201)
def report(sid: str, body: ReportIn, request: Request, u=Depends(maybe_user), db=DB):
    request.app.state.limiter.hit(f"report:{client_ip(request)}", 10, 3600)
    public(db, sid)
    reason = clean(body.reason, 500, "A reason", lines=True)
    if not reason:
        raise HTTPException(422, "Say what is wrong with it.")
    db.execute("INSERT OR IGNORE INTO reports (shard_id, reporter_id, reporter, reason, created_at) "
               "VALUES (?, ?, ?, ?, ?)", (sid, u and u["id"], f"u{u['id']}" if u else f"ip:{ip_hash(request)}",
                                          reason, now()))
    return {"ok": True}


# ---------------------------------------------------------------------------------------------- moderation
@r.get("/admin/reports")
def reports(all: bool = False, a=Depends(admin), db=DB):
    """the open reports (all=true: the resolved ones too), the newest first, each with its shard"""
    rows = db.execute("SELECT r.*, s.name, s.description, s.visibility, s.data, s.owner_id, u.display_name AS author, "
                      "u.banned_at FROM reports r JOIN shards s ON s.id = r.shard_id JOIN users u ON u.id = s.owner_id "
                      "WHERE ? OR r.resolved_at IS NULL ORDER BY r.created_at DESC, r.id DESC LIMIT 500", (all,))
    return {"items": [{"id": x["id"], "reason": x["reason"], "created_at": iso(x["created_at"]),
                       "resolved_at": iso(x["resolved_at"]), "by_user": x["reporter_id"],
                       "shard": {"id": x["shard_id"], "name": x["name"], "description": x["description"],
                                 "visibility": x["visibility"], "data": base64.b64encode(x["data"]).decode(),
                                 "author": x["author"], "owner_id": x["owner_id"],
                                 "owner_banned": bool(x["banned_at"])}} for x in rows]}


@r.post("/admin/reports/{rid}/resolve")
def resolve(rid: Id, a=Depends(admin), db=DB):
    db.execute("UPDATE reports SET resolved_at = COALESCE(resolved_at, ?) WHERE id = ?", (now(), rid))
    return {"ok": True}


def any_shard(db, sid):
    if not db.execute("SELECT 1 FROM shards WHERE id = ?", (sid,)).fetchone():
        raise HTTPException(404, "No such shard.")


@r.post("/admin/shards/{sid}/hide")
def hide(sid: str, a=Depends(admin), db=DB):
    """out of the River (its owner keeps it, and can't share it again); its reports resolved"""
    any_shard(db, sid)
    with tx(db):
        db.execute("UPDATE shards SET visibility = 'hidden' WHERE id = ?", (sid,))
        db.execute("UPDATE reports SET resolved_at = ? WHERE shard_id = ? AND resolved_at IS NULL", (now(), sid))
    return {"ok": True, "visibility": "hidden"}


@r.post("/admin/shards/{sid}/unhide")
def unhide(sid: str, a=Depends(admin), db=DB):
    any_shard(db, sid)
    db.execute("UPDATE shards SET visibility = 'public' WHERE id = ? AND visibility = 'hidden'", (sid,))
    return {"ok": True, "visibility": db.execute("SELECT visibility FROM shards WHERE id = ?", (sid,)).fetchone()[0]}


@r.post("/admin/shards/{sid}/delete")
def delete(sid: str, a=Depends(admin), db=DB):
    any_shard(db, sid)
    db.execute("DELETE FROM shards WHERE id = ?", (sid,))
    return {"ok": True}


def target(request, db, uid):
    t = db.execute("SELECT * FROM users WHERE id = ?", (uid,)).fetchone()
    if not t:
        raise HTTPException(404, "No such user.")
    if is_admin(t, request.app.state.settings):
        raise HTTPException(403, "An admin can't be banned.")
    return t


@r.post("/admin/users/{uid}/ban")
def ban(uid: Id, request: Request, a=Depends(admin), db=DB):
    """can't sign in, nor share; their shards leave the River (kept, should the ban be lifted)"""
    target(request, db, uid)
    with tx(db):
        db.execute("UPDATE users SET banned_at = COALESCE(banned_at, ?) WHERE id = ?", (now(), uid))
        db.execute("DELETE FROM sessions WHERE user_id = ?", (uid,))
    return {"ok": True, "banned": True}


@r.post("/admin/users/{uid}/unban")
def unban(uid: Id, request: Request, a=Depends(admin), db=DB):
    target(request, db, uid)
    db.execute("UPDATE users SET banned_at = NULL WHERE id = ?", (uid,))
    return {"ok": True, "banned": False}


# ---------------------------------------------------------------------------------------------- the export
@r.get("/me/export")
def export(request: Request, u=Depends(user), db=DB):
    """a zip: account.json (the account, its shards and backups as JSON), backups/*.frasil (as the FM-1 made them),
    shards-<layout>.frasilscenes (a scene package, as the River's page reads one)"""
    sch, s = request.app.state.schema, request.app.state.settings
    rows = db.execute(MINE + " ORDER BY s.created_at, s.id", (u["id"],)).fetchall()
    backs = db.execute("SELECT * FROM backups WHERE owner_id = ? ORDER BY created_at, id", (u["id"],)).fetchall()
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as z:
        files = {b["id"]: f"backups/{iso(b['created_at'])[:10]}-{b['id']}.frasil" for b in backs}
        for b in backs:
            z.writestr(files[b["id"]], b["image"])
        z.writestr("account.json", json.dumps({
            "account": {k: v for k, v in me_json(u, s).items() if k != "is_admin"},
            "shards": [shard_json(x) for x in rows],
            "backups": [{**backup_json(request, b), "file": files[b["id"]]} for b in backs]}, indent=1))
        here = [x for x in rows if x["layout"] == sch.layout]
        if here:
            z.writestr(f"shards-{sch.layout}.frasilscenes", sch.pack(here, here[-1]["firmware"] or sch.firmware))
    return Response(buf.getvalue(), media_type="application/zip", headers={
        "Content-Disposition": f'attachment; filename="river-{date.today().isoformat()}.zip"'})
