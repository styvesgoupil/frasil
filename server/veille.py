# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""The veille (admin.frasilsynth.com/veille/): what is said of FRASIL on the web, a mention a row, added to and never
replaced. A scheduled agent reads what is known and sends what it found, with its own token (Authorization: Bearer
FRASIL_VEILLE_TOKEN; Caddy lets those three routes past its basic auth): the token opens nothing else, and no token set
they are 404. A video or a thread and the comments under it go as one (the group, GROUP): the page's list and counts
are of those, and one of the owner's own comments among them puts what came before it at maybe_replied rather than to
be dealt with. The page reads and changes the mentions behind the admin's key, as admin.py's routes, and can post one
to Discord's #made-with-frasil with the server's bot (DISCORD_BOT_TOKEN): on its click only, never from the agent.

What the agent sends comes from the web: the page shows it as text, and its addresses are made https here (canon)."""
import hmac
import json
import re
import secrets
import unicodedata
from datetime import datetime, timezone
from typing import Annotated, Literal, get_args
from urllib.parse import parse_qsl, urlencode, urlsplit

import httpx
from fastapi import APIRouter, Depends, HTTPException, Query, Request
from pydantic import BaseModel, Field

from admin import keyed
from auth import DB
from guard import clean
from shards import Id
from store import iso, new_id, now, tx

Platform = Literal["youtube", "reddit", "github", "forum", "blog", "bluesky", "mastodon", "instagram", "tiktok", "x",
                   "facebook", "discord", "other"]
Kind = Literal["video", "post", "comment", "thread", "article", "repo", "issue", "other"]
Sentiment = Literal["positive", "neutral", "question", "bug", "negative"]
Status = Literal["new", "maybe_replied", "seen", "follow_up", "replied", "ignored", "keeper"]
Handled = ("replied", "ignored", "seen", "maybe_replied")     # (one of these whose engagement grows: new_activity)
BIG = 2 ** 62                                # (a number SQLite can hold)
URL_MAX, TITLE_MAX, NAME_MAX, TEXT_MAX, NOTES_MAX, QUOTE_MAX = 2048, 500, 200, 2000, 5000, 300
ITEMS_MAX, IDS_MAX = 1000, 500
KNOWN = ("platform", "kind", "title", "summary", "language", "sentiment", "priority", "parent_url")     # (to bring one up to date)
SAID_AS_FOUND = ("video", "post", "thread", "article", "repo", "issue")   # (their titles are the web's; the others' ours)
DISCORD_TITLE, DISCORD_MAX = 300, 2000
TRACKERS = {"si", "feature", "ref", "ref_src", "ref_url", "share_id", "fbclid", "gclid", "dclid", "msclkid", "yclid",
            "twclid", "igsh", "igshid", "mc_cid", "mc_eid"}
REDDIT = re.compile(r"(?:/r/([^/]+))?/comments/([a-z0-9]+)(?:/comment/([a-z0-9]+)|/[^/]*(?:/(?:comment/)?([a-z0-9]+))?)?",
                    re.I)
GITHUB = re.compile(r"/([^/]+)/([^/]+?)(?:\.git)?(/(?:issues|discussions|pull)/\d+)?(/.*)?")
TWEET = re.compile(r"/[^/]+/status/\d+")
VIDEO = re.compile(r"/(?:shorts|live|embed|v)/([\w-]+)")


def canon(url):
    """the one form of a mention's address, the key it is kept under: https, the host in lowercase without www., no
    fragment, no tracking, no final /; YouTube, Reddit and GitHub as their pages are named. None: no web address"""
    u = str(url or "").strip()
    if not u or len(u) > URL_MAX or re.search(r"\s|[\x00-\x1f\x7f]", u):
        return None
    try:
        s = urlsplit(u if urlsplit(u).scheme else "https://" + u.lstrip("/"))
        host, port = (s.hostname or "").lower().rstrip("."), s.port
    except ValueError:
        return None
    if s.scheme.lower() not in ("http", "https") or not host:
        return None
    host = host.removeprefix("www.")
    path, kept = s.path.rstrip("/"), parse_qsl(s.query, keep_blank_values=True)
    q = dict(kept)
    if host == "youtu.be" or host in ("youtube.com", "m.youtube.com", "music.youtube.com", "youtube-nocookie.com"):
        video = VIDEO.fullmatch(path)
        v = path[1:].split("/")[0] if host == "youtu.be" else video[1] if video else q.get("v") if path == "/watch" else 0
        if v:
            return f"https://www.youtube.com/watch?{urlencode({'v': v, **({'lc': q['lc']} if q.get('lc') else {})})}"
        host = "www.youtube.com"
    elif host == "redd.it" or host == "reddit.com" or host.endswith(".reddit.com"):
        r = REDDIT.fullmatch("/comments" + path if host == "redd.it" else path)
        if r:
            sub, post, cid = r[1], r[2].lower(), r[3] or r[4]
            where = f"/r/{sub.lower()}" if sub else ""
            return f"https://www.reddit.com{where}/comments/{post}/" + (f"comment/{cid.lower()}/" if cid else "")
        host, kept = "www.reddit.com", []
    elif host == "github.com":
        g = GITHUB.fullmatch(path)
        if g:
            return f"https://github.com/{g[1].lower()}/{g[2].lower()}{g[3] or g[4] or ''}"
        kept = []
    elif host in ("x.com", "mobile.x.com", "twitter.com", "mobile.twitter.com"):
        host, kept, path = "x.com", [], path.lower()
        tweet = TWEET.match(path)
        path = tweet[0] if tweet else path
    elif host in ("instagram.com", "tiktok.com"):      # (nothing in their queries says what is shown)
        kept = []
    kept = [(k, v) for k, v in kept if k.lower() not in TRACKERS and not k.lower().startswith("utm_")]
    return f"https://{host}{f':{port}' if port not in (None, 80, 443) else ''}{path}" + \
        (f"?{urlencode(kept)}" if kept else "")


class Moment(BaseModel):
    at: int = Field(ge=0, le=86400)
    quote: str = ""


class Item(BaseModel):
    url: str
    platform: Platform
    kind: Kind
    title: str
    author: str | None = None
    community: str | None = None
    summary: str
    quote: str | None = None                                    # (what was said, word for word: the page shows it first)
    language: str
    sentiment: Sentiment
    priority: int = Field(ge=1, le=3)
    parent_url: str | None = None
    parent_title: str | None = None
    published_at: datetime | None = None
    engagement: int | None = Field(None, ge=0, le=BIG)
    is_own: bool = False
    moments: list[Moment] | None = Field(None, max_length=20)      # (a video: where it is said; none: as they were)


class Run(BaseModel):
    started_at: datetime | None = None


class Ingest(BaseModel):
    run: Run = Run()
    items: list[Item] = Field(default=[], max_length=ITEMS_MAX)


class Finish(BaseModel):
    status: Literal["ok", "partial", "error"]
    notes: str = ""


class Change(BaseModel):
    status: Status | None = None
    notes: str | None = None
    new_activity: bool | None = None


class Bulk(BaseModel):
    ids: list[Annotated[int, Field(ge=1, le=BIG)]] = Field(min_length=1, max_length=IDS_MAX)
    status: Status
    new_activity: bool | None = None


def bearer(request: Request):
    k = request.app.state.settings.veille_token
    if not k:
        raise HTTPException(404, "Not Found")
    if not hmac.compare_digest(request.headers.get("authorization", "").encode(), f"Bearer {k}".encode()):
        raise HTTPException(401, "Wrong token.", headers={"WWW-Authenticate": "Bearer"})


GROUP = "COALESCE(m.parent_url, m.url)"       # (a comment goes with what it is under)
DAY = "(m.first_found_at - :tz) / 86400"
AT = "COALESCE(m.published_at, m.first_found_at)"
agent = APIRouter(prefix="/api/veille", dependencies=[Depends(bearer)])
page = APIRouter(prefix="/api/veille", dependencies=[Depends(keyed)])


def unix(d, what="started_at"):
    """a time as kept (one without a zone is UTC's), or a 422: iso() must be able to give it back"""
    if d is None:
        return None
    t = int((d if d.tzinfo else d.replace(tzinfo=timezone.utc)).timestamp())
    if not 0 <= t < 2 ** 33:
        raise HTTPException(422, f"{what}: out of range.")
    return t


def row_of(i, n):
    """what item n of a batch will be kept as, or a 422 naming it"""
    where = f"items.{n}"
    url, parent = canon(i.url), canon(i.parent_url) if i.parent_url else None
    if not url or (i.parent_url and not parent):
        raise HTTPException(422, f"{where}: {'url' if not url else 'parent_url'} isn't a web address.")
    title, summary = clean(i.title, TITLE_MAX, f"{where}.title"), clean(i.summary, TEXT_MAX, f"{where}.summary", lines=True)
    lang = clean(i.language, 16, f"{where}.language").lower()
    if not title or not summary or not lang:
        raise HTTPException(422, f"{where}: title, summary and language can't be empty.")
    moments = json.dumps([{"at": x.at, "quote": clean(x.quote, QUOTE_MAX, f"{where}.moments")}
                          for x in sorted(i.moments, key=lambda x: x.at)], ensure_ascii=False) if i.moments else None
    return {"url": url, "platform": i.platform, "kind": i.kind, "title": title,
            "author": clean(i.author, NAME_MAX, f"{where}.author") or None,
            "community": clean(i.community, NAME_MAX, f"{where}.community") or None, "summary": summary,
            "language": lang, "sentiment": i.sentiment, "priority": i.priority, "parent_url": parent,
            "parent_title": clean(i.parent_title, TITLE_MAX, f"{where}.parent_title") or None,
            "quote": clean(i.quote, TEXT_MAX, f"{where}.quote", lines=True) or None,
            "published_at": unix(i.published_at, f"{where}.published_at"), "engagement": i.engagement, "is_own": int(i.is_own), "moments": moments}


def answered(db, group, t):
    """the owner has spoken in this group: what is new in it and came before that is maybe_replied; what came after, and
    whatever has been dealt with since, stays as it is"""
    db.execute(f"UPDATE mentions AS m SET status = 'maybe_replied', status_changed_at = :t WHERE m.status = 'new' AND "
               f"{GROUP} = :k AND {AT} <= (SELECT MAX(COALESCE(o.published_at, o.first_found_at)) FROM mentions o "
               f"WHERE o.is_own = 1 AND COALESCE(o.parent_url, o.url) = :k)", {"t": t, "k": group})


@agent.get("/known")
def known(db=DB):
    """every mention kept, the last seen first: what not to work out again, what to watch for new activity"""
    rows = db.execute(f"SELECT url, status, engagement, last_seen_at, {', '.join(KNOWN)} FROM mentions "
                      "ORDER BY last_seen_at DESC, id DESC")
    return {"items": [{"url": x["url"], "status": x["status"], "engagement": x["engagement"],
                       "last_seen_at": iso(x["last_seen_at"]), **{k: x[k] for k in KNOWN}} for x in rows]}


@agent.post("/ingest")
def ingest(body: Ingest, db=DB):
    """{run: {started_at}, items: [...]} -> {run_id, inserted, updated}: a new address is added as new; one kept has
    what the web says of it brought up to date (what was unknown filled in), never what was done with it (status,
    notes, first_found_at), and if it was dealt with and its engagement grew, new_activity. An engagement unknown until
    now is no growth. All of it, or (a 422 naming the item) nothing"""
    rows, t, inserted, updated = [row_of(i, n) for n, i in enumerate(body.items)], now(), 0, 0
    own = {m["parent_url"] for m in rows if m["is_own"] and m["parent_url"]}    # (the owner's replies: no new activity)
    with tx(db):
        run = new_id(db, "mention_runs")
        db.execute("INSERT INTO mention_runs (id, started_at, items_sent) VALUES (?, ?, ?)",
                   (run, unix(body.run.started_at) or t, len(rows)))
        for m in rows:
            was = db.execute("SELECT id, status, engagement FROM mentions WHERE url = ?", (m["url"],)).fetchone()
            if not was:
                db.execute("INSERT INTO mentions (url, platform, kind, title, author, community, summary, language, "
                           "sentiment, priority, parent_url, parent_title, published_at, engagement, is_own, moments, quote, "
                           "status_changed_at, first_found_at, last_seen_at, last_run_id) VALUES (:url, :platform, :kind, "
                           ":title, :author, :community, :summary, :language, :sentiment, :priority, :parent_url, "
                           ":parent_title, :published_at, :engagement, :is_own, :moments, :quote, :t, :t, :t, :run)",
                           {**m, "t": t, "run": run})
                inserted += 1
                continue
            grew = (was["status"] in Handled and was["engagement"] is not None and m["engagement"] is not None
                    and m["engagement"] > was["engagement"] and (m["parent_url"] or m["url"]) not in own)
            db.execute("UPDATE mentions SET title = :title, summary = :summary, sentiment = :sentiment, "
                       "priority = :priority, engagement = COALESCE(:engagement, engagement), last_seen_at = :t, "
                       "last_run_id = :run, new_activity = new_activity OR :grew, author = COALESCE(author, :author), "
                       "community = COALESCE(community, :community), parent_url = COALESCE(parent_url, :parent_url), "
                       "parent_title = COALESCE(:parent_title, parent_title), quote = COALESCE(:quote, quote), "
                       "published_at = COALESCE(published_at, :published_at), moments = COALESCE(:moments, moments) "
                       "WHERE id = :id",
                       {**m, "t": t, "run": run, "grew": int(grew), "id": was["id"]})
            updated += 1
        for k in own:
            answered(db, k, t)
        db.execute("UPDATE mention_runs SET inserted = ?, updated = ? WHERE id = ?", (inserted, updated, run))
    return {"run_id": run, "inserted": inserted, "updated": updated}


@agent.post("/runs/{run_id}/finish")
def finish(run_id: str, body: Finish, db=DB):
    """{status: ok | partial | error, notes} (what failed, say): a run ended"""
    notes = clean(body.notes, TEXT_MAX, "notes", lines=True)
    n = db.execute("UPDATE mention_runs SET finished_at = ?, status = ?, notes = ? WHERE id = ?",
                   (now(), body.status, notes, run_id)).rowcount
    if not n:
        raise HTTPException(404, "No such run.")
    return {"ok": True}


# ------------------------------------------------------------ the page's (website/admin/veille.js)
VIEWS = {"inbox": "(m.status = 'new' OR m.new_activity = 1)", "activity": "m.new_activity = 1"}
CURSOR = re.compile(r"\d+\.\d+\.-?\d+\.\d+")


def fold(text):
    """lowercase, no accents: what a search compares"""
    return "".join(c for c in unicodedata.normalize("NFKD", str(text or "")).casefold() if not unicodedata.combining(c))


def counts(db, within="1", args=None):
    """of groups: one with a mention in two statuses counts in both; within: the page's filters"""
    args = args or {}
    n = {x["status"]: x["n"] for x in db.execute(f"SELECT status, COUNT(DISTINCT {GROUP}) AS n FROM mentions m "
                                                 f"WHERE {within} GROUP BY status", args)}
    one = lambda where: db.execute(f"SELECT COUNT(DISTINCT {GROUP}) FROM mentions m WHERE ({within}) AND {where}",
                                   args).fetchone()[0]
    return {**{s: n.get(s, 0) for s in get_args(Status)}, "new_activity": one(VIEWS["activity"]),
            "inbox": one(VIEWS["inbox"]), "total": one("1")}


def at_moment(url, at):
    return f"{url}&t={at}s" if url.startswith("https://www.youtube.com/watch?v=") and "&lc=" not in url else url


def mention_json(m):
    return {"id": m["id"], "url": m["url"], "platform": m["platform"], "kind": m["kind"], "title": m["title"],
            "author": m["author"], "community": m["community"], "summary": m["summary"], "language": m["language"],
            "sentiment": m["sentiment"], "priority": m["priority"], "parent_url": m["parent_url"],
            "published_at": iso(m["published_at"]), "engagement": m["engagement"], "is_own": bool(m["is_own"]),
            "status": m["status"], "status_changed_at": iso(m["status_changed_at"]), "notes": m["notes"],
            "new_activity": bool(m["new_activity"]), "first_found_at": iso(m["first_found_at"]),
            "last_seen_at": iso(m["last_seen_at"]), "last_run_id": m["last_run_id"], "discord_url": m["discord_url"] or None,
            "moments": [{**x, "url": at_moment(m["url"], x["at"])} for x in json.loads(m["moments"] or "[]")],
            "parent_title": m["parent_title"], "quote": m["quote"],
            "group": m["parent_url"] or m["url"]}


def last_run(db):
    r = db.execute("SELECT * FROM mention_runs ORDER BY rowid DESC LIMIT 1").fetchone()
    return r and {"id": r["id"], "started_at": iso(r["started_at"]), "finished_at": iso(r["finished_at"]),
                  "status": r["status"], "items_sent": r["items_sent"], "inserted": r["inserted"],
                  "updated": r["updated"], "notes": r["notes"]}


@page.get("/mentions")
def mentions(request: Request, db=DB, status: str = "inbox", platform: Platform | None = None, sentiment: Sentiment | None = None,
             priority: int | None = Query(None, ge=1, le=3), q: str = Query("", max_length=200),
             cursor: str = "", tz: int = Query(0, ge=-840, le=840), limit: int = Query(50, ge=1, le=200)):
    """status: inbox (new, or with new activity), activity, all, or one status; platform, sentiment, priority; q: words
    in the title, summary, author or community, accents and case aside. By groups: any mention of one that fits brings
    all of it, what it is under first, then its comments as they came; limit and cursor count groups. By the day found
    (tz: the page's getTimezoneOffset), then the priority, then the date. Also what the page's head shows: counts,
    platforms, last_run; tabs: the counts under these filters"""
    s = request.app.state.settings
    where, view, args = [], None, {"tz": tz * 60, "n": limit + 1}
    if status in VIEWS:
        view = VIEWS[status]
    elif status in get_args(Status):
        view = "m.status = :status"
        args["status"] = status
    elif status != "all":
        raise HTTPException(422, "status: inbox, activity, all, or a mention's status.")
    for k, v in (("platform", platform), ("sentiment", sentiment), ("priority", priority)):
        if v is not None:
            where.append(f"m.{k} = :{k}")
            args[k] = v
    db.create_function("fold", 1, fold, deterministic=True)
    for n, word in enumerate(fold(q).split()):
        where.append(f"fold(m.title || ' ' || m.summary || ' ' || COALESCE(m.author, '') || ' ' || "
                     f"COALESCE(m.community, '') || ' ' || COALESCE(m.parent_title, '') || ' ' || COALESCE(m.quote, '')) "
                     f"LIKE :w{n} ESCAPE '\\'")
        args[f"w{n}"] = "%" + word.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_") + "%"
    tabs = counts(db, " AND ".join(where) or "1", args)     # (the statuses' tabs, under the page's filters)
    where += [view] if view else []
    if cursor:
        if not CURSOR.fullmatch(cursor):
            raise HTTPException(422, "cursor: not one of ours.")
        args.update({f"c{n}": int(x) for n, x in enumerate(cursor.split("."))})
    groups = db.execute(f"SELECT * FROM (SELECT {GROUP} AS k, MAX({DAY}) AS d, MAX(m.priority) AS p, MAX({AT}) AS a, "
                        f"MAX(m.id) AS i FROM mentions m WHERE {' AND '.join(where) or '1'} GROUP BY k) "
                        f"{'WHERE (d, p, a, i) < (:c0, :c1, :c2, :c3)' if cursor else ''} "
                        "ORDER BY d DESC, p DESC, a DESC, i DESC LIMIT :n", args).fetchall()
    more, groups = len(groups) > limit, groups[:limit]
    last, within = groups[-1] if groups else None, {g["k"]: [] for g in groups}
    if within:
        marks = ", ".join("?" * len(within))
        for m in db.execute(f"SELECT m.* FROM mentions m WHERE {GROUP} IN ({marks}) ORDER BY m.parent_url IS NOT NULL, "
                            f"{AT}, m.id", list(within)):
            within[m["parent_url"] or m["url"]].append(m)
    day = lambda g: datetime.fromtimestamp(g["d"] * 86400, timezone.utc).strftime("%Y-%m-%d")
    return {"items": [{**mention_json(m), "day": day(g)} for g in groups for m in within[g["k"]]],
            "next_cursor": f"{last['d']}.{last['p']}.{last['a']}.{last['i']}" if more else None,
            "counts": counts(db), "tabs": tabs, "last_run": last_run(db), "discord": bool(s.discord_token),
            "platforms": {x["platform"]: x["n"] for x in db.execute(
                f"SELECT platform, COUNT(DISTINCT {GROUP}) AS n FROM mentions m GROUP BY platform ORDER BY n DESC, platform")}}


def apply(db, ids, status=None, notes=None, activity=None):
    """a status set clears new_activity, unless activity says otherwise (UNDO); status_changed_at moves if it changed"""
    sets, args = [], {"t": now()}
    if status:
        sets += ["status_changed_at = CASE WHEN status != :status THEN :t ELSE status_changed_at END", "status = :status"]
        args["status"] = status
        activity = bool(activity)
    if notes is not None:
        sets.append("notes = :notes")
        args["notes"] = clean(notes, NOTES_MAX, "notes", lines=True)
    if activity is not None:
        sets.append("new_activity = :activity")
        args["activity"] = int(activity)
    if not sets:
        return 0
    marks = ", ".join(f":i{n}" for n in range(len(ids)))
    args.update({f"i{n}": i for n, i in enumerate(ids)})
    with tx(db):
        return db.execute(f"UPDATE mentions SET {', '.join(sets)} WHERE id IN ({marks})", args).rowcount


@page.patch("/mentions/{mid}")
def change(mid: Id, body: Change, db=DB):
    """{status?, notes?, new_activity?} -> {item, counts}"""
    if not db.execute("SELECT 1 FROM mentions WHERE id = ?", (mid,)).fetchone():
        raise HTTPException(404, "No such mention.")
    apply(db, [mid], body.status, body.notes, body.new_activity)
    m = db.execute("SELECT * FROM mentions WHERE id = ?", (mid,)).fetchone()
    return {"item": mention_json(m), "counts": counts(db)}


@page.patch("/mentions")
def change_all(body: Bulk, db=DB):
    """{ids, status, new_activity?} -> {updated, counts}"""
    return {"updated": apply(db, body.ids, body.status, None, body.new_activity), "counts": counts(db)}


# ------------------------------------------------------------ a mention in #made-with-frasil
def discord_text(m):
    """the title as the web gave it and who made it, then the link Discord opens, at the first moment if it has any;
    never the summary (ours, in French).
    Words from the web are plain text there: no markdown, and the post itself allows no mention"""
    plain = lambda t: re.sub(r"([\\*_~`|>#\[\]()-])", r"\\\1", t).replace("://", r"\://")     # (no live link either)
    title = m["title"][:DISCORD_TITLE] if m["kind"] in SAID_AS_FOUND else None
    head = " · ".join(filter(None, (title and f"**{plain(title)}**", m["author"] and plain(m["author"]))))
    moments = json.loads(m["moments"] or "[]")
    url = at_moment(m["url"], moments[0]["at"]) if moments else m["url"]
    text = f"{head}\n{url}" if head else url
    if len(text) > DISCORD_MAX:
        raise HTTPException(422, "Too long for Discord.")
    return text


def to_discord(request, method, path, **kw):
    s = request.app.state.settings
    try:
        return request.app.state.http.request(method, s.discord_api + path, headers={
            "Authorization": f"Bot {s.discord_token}", "User-Agent": "DiscordBot (https://frasilsynth.com, 1.0)"}, **kw)
    except httpx.HTTPError:
        raise HTTPException(502, "Discord didn't answer.")


def a_mention(db, mid):
    m = db.execute("SELECT * FROM mentions WHERE id = ?", (mid,)).fetchone()
    if not m:
        raise HTTPException(404, "No such mention.")
    return m


@page.post("/mentions/{mid}/discord")
def post_it(mid: Id, request: Request, db=DB):
    """-> {item}: posted once (409 after that, a second click on its way included), its message's address kept. Refused:
    it can be tried again. No clear answer (it may have gone): tried again with the same nonce, Discord doesn't repeat it"""
    s = request.app.state.settings
    if not s.discord_token:
        raise HTTPException(404, "Not Found")
    text = discord_text(a_mention(db, mid))
    if not db.execute("UPDATE mentions SET discord_url = '', discord_nonce = COALESCE(discord_nonce, ?) WHERE id = ? AND "
                      "discord_url IS NULL", (secrets.token_hex(8), mid)).rowcount:
        raise HTTPException(409, "Already on Discord.")
    url, nonce = None, a_mention(db, mid)["discord_nonce"]
    try:
        r = to_discord(request, "POST", f"/channels/{s.discord_channel}/messages",
                       json={"content": text, "allowed_mentions": {"parse": []}, "nonce": nonce, "enforce_nonce": True})
        if r.status_code == 200:
            try:
                url = f"https://discord.com/channels/{s.discord_guild}/{s.discord_channel}/{r.json()['id']}"
            except (ValueError, KeyError):
                raise HTTPException(502, "Discord's answer can't be read.")
        else:
            nonce = None                              # (refused: nothing went)
            raise HTTPException(502, f"Discord answered {r.status_code}.")
        nonce = None
    finally:
        db.execute("UPDATE mentions SET discord_url = ?, discord_nonce = ? WHERE id = ?", (url, nonce, mid))
    return {"item": mention_json(a_mention(db, mid))}


@page.delete("/mentions/{mid}/discord")
def take_it_back(mid: Id, request: Request, db=DB):
    """UNDO: its message gone from the channel (gone already: the same) -> {item}"""
    u = a_mention(db, mid)["discord_url"]
    if not u:
        raise HTTPException(404, "Not on Discord.")
    channel, message = u.rsplit("/", 2)[1:]
    r = to_discord(request, "DELETE", f"/channels/{channel}/messages/{message}")
    if r.status_code not in (200, 204, 404):
        raise HTTPException(502, f"Discord answered {r.status_code}.")
    db.execute("UPDATE mentions SET discord_url = NULL WHERE id = ?", (mid,))
    return {"item": mention_json(a_mention(db, mid))}
