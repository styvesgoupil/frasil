# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""The River's settings (the environment: .env.example lists them) and its database (SQLite in WAL mode, one file in
the volume: /data/river.sqlite). Times are Unix seconds; the API gives them as ISO 8601 (iso)."""
import os
import secrets
import sqlite3
import time
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent


def env_list(name, default=""):
    return [x.strip() for x in os.environ.get(name, default).split(",") if x.strip()]


@dataclass
class Settings:
    db: str = "river.sqlite"
    url: str = "https://river.frasilsynth.com"     # the site's origin: the links in emails, the Origin to accept
    schema: str = str(HERE / "schema.json")
    smtp_host: str = ""
    smtp_port: int = 465
    smtp_user: str = ""
    smtp_password: str = ""
    mail_from: str = "FRASIL River <noreply@frasilsynth.com>"
    reply_to: str = "hi@frasilsynth.com"          # where a reply goes (noreply@ reads nothing); empty: none
    google_id: str = ""
    google_secret: str = ""
    google_redirect: str = ""
    google_auth_url: str = "https://accounts.google.com/o/oauth2/v2/auth"
    google_token_url: str = "https://oauth2.googleapis.com/token"
    admins: list = field(default_factory=list)
    words: list | None = None                     # the word filter's (None: guard.WORDS)
    ip_header: str = "cf-connecting-ip"           # where the client's address is (Cloudflare's, through Caddy)
    cookie_secure: bool = True
    static: str = ""                              # a folder served at / (local runs only: Caddy does it on nimbus)
    site_url: str = "https://frasilsynth.com"     # the main site: the other Origin a waitlist signup comes from
    admin_key: str = field(default="", repr=False)   # /api/admin/ (admin.py): X-Frasil-Admin; empty: 404
    admin_url: str = "https://admin.frasilsynth.com"   # the admin page: the Origin the veille's changes come from
    veille_token: str = field(default="", repr=False)  # the veille's agent (veille.py): Bearer; empty: 404
    discord_token: str = field(default="", repr=False) # the bot that posts a mention (veille.py); empty: no such action
    discord_api: str = "https://discord.com/api/v10"
    discord_guild: str = "1563986548209356830"         # the server Frasil Synth
    discord_channel: str = "1563988202677739541"       # its #made-with-frasil

    @classmethod
    def from_env(cls):
        e = os.environ.get
        url = (e("RIVER_URL") or cls.url).rstrip("/")
        words = env_list("WORD_FILTER")
        return cls(db=e("RIVER_DB") or cls.db, url=url, schema=e("RIVER_SCHEMA") or cls.schema,
                   smtp_host=e("SMTP_HOST", ""), smtp_port=int(e("SMTP_PORT") or 465), smtp_user=e("SMTP_USER", ""),
                   smtp_password=e("SMTP_PASSWORD", ""), mail_from=e("MAIL_FROM") or cls.mail_from,
                   reply_to=e("MAIL_REPLY_TO", cls.reply_to),
                   google_id=e("GOOGLE_CLIENT_ID", ""), google_secret=e("GOOGLE_CLIENT_SECRET", ""),
                   google_redirect=e("GOOGLE_REDIRECT_URI") or f"{url}/api/auth/google/callback",
                   admins=[a.lower() for a in env_list("ADMIN_EMAILS")], words=words or None,
                   ip_header=e("CLIENT_IP_HEADER", cls.ip_header).lower(),
                   cookie_secure=e("COOKIE_SECURE", "1") != "0", static=e("RIVER_STATIC", ""),
                   site_url=(e("SITE_URL") or cls.site_url).rstrip("/"), admin_key=e("FRASIL_ADMIN_KEY", "").strip(),
                   admin_url=(e("ADMIN_URL") or cls.admin_url).rstrip("/"),
                   veille_token=e("FRASIL_VEILLE_TOKEN", "").strip(), discord_token=e("DISCORD_BOT_TOKEN", "").strip(),
                   discord_api=(e("DISCORD_API") or cls.discord_api).rstrip("/"),
                   discord_guild=e("DISCORD_GUILD") or cls.discord_guild,
                   discord_channel=e("DISCORD_MADE_CHANNEL") or cls.discord_channel)


SQL = """
CREATE TABLE IF NOT EXISTS meta (k TEXT PRIMARY KEY, v TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS users (
  id INTEGER PRIMARY KEY, email TEXT NOT NULL UNIQUE, display_name TEXT NOT NULL, password TEXT,
  created_at INTEGER NOT NULL, verified_at INTEGER, banned_at INTEGER);
CREATE TABLE IF NOT EXISTS sessions (           -- the cookie's token: only its SHA-256 here
  hash TEXT PRIMARY KEY, user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  created_at INTEGER NOT NULL, expires_at INTEGER NOT NULL);
CREATE INDEX IF NOT EXISTS sessions_user ON sessions(user_id);
CREATE TABLE IF NOT EXISTS tokens (             -- one-time links: verify (48 h), reset (1 h); their SHA-256
  hash TEXT PRIMARY KEY, user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  kind TEXT NOT NULL, expires_at INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS oauth (              -- a Google sign-in under way (10 min): its state's SHA-256
  hash TEXT PRIMARY KEY, verifier TEXT NOT NULL, nonce TEXT NOT NULL, next TEXT NOT NULL, expires_at INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS shards (
  id TEXT PRIMARY KEY, owner_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  name TEXT NOT NULL, title TEXT NOT NULL DEFAULT '', description TEXT NOT NULL DEFAULT '',
  tags TEXT NOT NULL DEFAULT '[]',                -- (JSON; title and tags: no longer used, emptied by untitled)
  layout TEXT NOT NULL, firmware TEXT NOT NULL DEFAULT '', data BLOB NOT NULL, from_slot INTEGER,
  root INTEGER, scale INTEGER, type INTEGER, drone INTEGER, glint INTEGER, bseq INTEGER,
  visibility TEXT NOT NULL DEFAULT 'private', license TEXT NOT NULL DEFAULT 'CC BY 4.0',
  origin_id TEXT,                                 -- saved from that public shard (it may be gone since)
  origin_author_id INTEGER REFERENCES users(id) ON DELETE SET NULL,   -- whose it first was (CC BY: credited)
  created_at INTEGER NOT NULL, updated_at INTEGER NOT NULL, shared_at INTEGER,
  plays INTEGER NOT NULL DEFAULT 0, sends INTEGER NOT NULL DEFAULT 0, saves INTEGER NOT NULL DEFAULT 0);
CREATE INDEX IF NOT EXISTS shards_owner ON shards(owner_id, created_at);
CREATE INDEX IF NOT EXISTS shards_public ON shards(visibility, shared_at);
CREATE INDEX IF NOT EXISTS shards_origin ON shards(origin_id);
CREATE TABLE IF NOT EXISTS backups (
  id TEXT PRIMARY KEY, owner_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  created_at INTEGER NOT NULL, note TEXT NOT NULL DEFAULT '', firmware TEXT NOT NULL DEFAULT '', image BLOB NOT NULL);
CREATE INDEX IF NOT EXISTS backups_owner ON backups(owner_id, created_at);
CREATE TABLE IF NOT EXISTS reports (
  id INTEGER PRIMARY KEY, shard_id TEXT NOT NULL REFERENCES shards(id) ON DELETE CASCADE,
  reporter_id INTEGER REFERENCES users(id) ON DELETE CASCADE, reporter TEXT NOT NULL,   -- (a user, or a hashed IP)
  reason TEXT NOT NULL, created_at INTEGER NOT NULL, resolved_at INTEGER, UNIQUE (shard_id, reporter));
CREATE TABLE IF NOT EXISTS saved (              -- who saved which shard: its saves count each user once
  shard_id TEXT NOT NULL REFERENCES shards(id) ON DELETE CASCADE,
  user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, PRIMARY KEY (shard_id, user_id)) WITHOUT ROWID;
CREATE TABLE IF NOT EXISTS favorites (          -- a bookmark to someone else's shard (not a copy)
  shard_id TEXT NOT NULL REFERENCES shards(id) ON DELETE CASCADE,
  user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, created_at INTEGER NOT NULL,
  PRIMARY KEY (shard_id, user_id));
CREATE INDEX IF NOT EXISTS favorites_user ON favorites(user_id, created_at);
CREATE TABLE IF NOT EXISTS waitlist (           -- who wants to hear when FRASIL for iPad or the VST is out (nothing sent)
  id INTEGER PRIMARY KEY, list TEXT NOT NULL, email TEXT NOT NULL, created INTEGER NOT NULL, UNIQUE (list, email));
CREATE INDEX IF NOT EXISTS waitlist_new ON waitlist(list, created);
CREATE TABLE IF NOT EXISTS hits (               -- the counters' once a day per listener (a user, or a hashed IP)
  shard_id TEXT NOT NULL, kind TEXT NOT NULL, who TEXT NOT NULL, day INTEGER NOT NULL,
  PRIMARY KEY (shard_id, kind, who, day)) WITHOUT ROWID;
CREATE TABLE IF NOT EXISTS mention_runs (       -- the veille's passes (veille.py); status: none while one runs
  id TEXT PRIMARY KEY, started_at INTEGER NOT NULL, finished_at INTEGER, status TEXT,
  items_sent INTEGER NOT NULL DEFAULT 0, inserted INTEGER NOT NULL DEFAULT 0, updated INTEGER NOT NULL DEFAULT 0,
  notes TEXT NOT NULL DEFAULT '');
CREATE TABLE IF NOT EXISTS mentions (           -- what is said of FRASIL on the web (url: canon()'s, one form of it)
  id INTEGER PRIMARY KEY, url TEXT NOT NULL UNIQUE, platform TEXT NOT NULL, kind TEXT NOT NULL, title TEXT NOT NULL,
  author TEXT, community TEXT, summary TEXT NOT NULL, language TEXT NOT NULL, sentiment TEXT NOT NULL,
  priority INTEGER NOT NULL, parent_url TEXT, published_at INTEGER, engagement INTEGER,
  is_own INTEGER NOT NULL DEFAULT 0, status TEXT NOT NULL DEFAULT 'new', status_changed_at INTEGER NOT NULL,
  notes TEXT NOT NULL DEFAULT '', new_activity INTEGER NOT NULL DEFAULT 0, first_found_at INTEGER NOT NULL,
  last_seen_at INTEGER NOT NULL, last_run_id TEXT REFERENCES mention_runs(id) ON DELETE SET NULL,
  discord_url TEXT,                             -- its post in #made-with-frasil ('': one on its way)
  moments TEXT,                                 -- where a video says it (JSON: [{at: seconds, quote}])
  parent_title TEXT,                            -- what a comment is under, when that isn't kept itself
  discord_nonce TEXT,                           -- a post that may have gone: its nonce, for Discord to know it again
  quote TEXT);                                  -- what was said, word for word (summary: ours, when there is none)
CREATE INDEX IF NOT EXISTS mentions_status ON mentions(status, new_activity);
CREATE INDEX IF NOT EXISTS mentions_found ON mentions(first_found_at);
"""


def now():
    return int(time.time())


def iso(t):
    return None if t is None else datetime.fromtimestamp(t, timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def popular(plays, sends, kept, shared_at, t):
    """the charts' score: who kept it counts most, then sends, then plays, over the shard's age ((days + 2) ^ 1.5: a
    week-old shard needs about 10 times the listeners of a new one). kept (shards.KEPT): its saves (v1's copies: each
    user once, ever, the copy deleted or not), then its favorites from users who hadn't saved it. A favorite weighs
    what a save did, the saves made before favorites still count, and no one counts twice"""
    days = max(0, t - (shared_at or t)) / 86400
    return (1 + plays + 3 * sends + 5 * kept) / (days + 2) ** 1.5


def connect(path):
    # autocommit (`with tx(db)` for several writes); FastAPI may hand a request's connection from thread to thread
    db = sqlite3.connect(path, timeout=10, isolation_level=None, check_same_thread=False)
    db.row_factory = sqlite3.Row
    db.execute("PRAGMA foreign_keys = ON")
    db.execute("PRAGMA busy_timeout = 5000")
    db.execute("PRAGMA synchronous = NORMAL")
    db.create_function("popular", 5, popular, deterministic=True)
    return db


def init(path, schema=None):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    db = connect(path)
    db.execute("PRAGMA journal_mode = WAL")
    db.executescript(SQL)
    if "bseq" not in [c["name"] for c in db.execute("PRAGMA table_info(shards)")]:
        db.execute("ALTER TABLE shards ADD COLUMN bseq INTEGER")    # (a database from before the bass's filter)
    have = [c["name"] for c in db.execute("PRAGMA table_info(mentions)")]
    for c in ("discord_url", "moments", "parent_title", "discord_nonce", "quote"):      # (a database from before them)
        if c not in have:
            db.execute(f"ALTER TABLE mentions ADD COLUMN {c} TEXT")
    db.execute("UPDATE mentions SET discord_url = NULL WHERE discord_url = ''")     # (a post cut off; one worker: none under way)
    db.execute("INSERT OR IGNORE INTO meta (k, v) VALUES ('salt', ?)", (secrets.token_hex(16),))
    salt = db.execute("SELECT v FROM meta WHERE k = 'salt'").fetchone()[0]
    housekeep(db)
    untitled(db)
    if schema:
        upgrade(db, schema)
    db.close()
    return salt


def upgrade(db, schema):
    """the shards kept in an earlier layout (FRSk: SCATTER where GLINT's RELEASE is; FRSi: before BASS's DECAY,
    SUSTAIN and PULSE too; FRSh: before the bass's sequence too; FRSg: names of ten), made over into the schema's as the
    FM-1 would (scenes.Schema.upgraded: the new settings at their defaults, RELEASE as DECAY let a glint go then, the
    bass empty, the name NUL-padded); the others left as they are. Backups stay as the
    FM-1 made them (it makes an earlier layout over itself, restored). Then the shards of the schema's layout without
    their bseq (from before the bass's filter) given it"""
    with tx(db):
        for x in db.execute("SELECT id, layout, data FROM shards WHERE layout != ?", (schema.layout,)).fetchall():
            data = schema.upgraded(x["data"], x["layout"])
            if data is not None:
                db.execute("UPDATE shards SET layout = ?, data = ?, bseq = ? WHERE id = ?",
                           (schema.layout, data, schema.derived(data)["bseq"], x["id"]))
        for x in db.execute("SELECT id, data FROM shards WHERE layout = ? AND bseq IS NULL",
                            (schema.layout,)).fetchall():
            db.execute("UPDATE shards SET bseq = ? WHERE id = ?", (schema.derived(x["data"])["bseq"], x["id"]))


def untitled(db):
    """no more titles nor tags (River v2: a name and a description): a title becomes its description's first sentence
    (unless the description starts with it already; 1000 characters at most), then is emptied; the tags too. Once:
    nothing is left to do the next time"""
    with tx(db):
        for x in db.execute("SELECT id, title, description FROM shards WHERE title != ''").fetchall():
            t, d = x["title"].strip(), x["description"]
            if t and not d.casefold().startswith(t.casefold()):
                d = t if not d else (t if t[-1] in ".!?…" else t + ".") + " " + d
                d = d if len(d) <= 1000 else d[:999].rstrip() + "…"
            db.execute("UPDATE shards SET title = '', description = ? WHERE id = ?", (d, x["id"]))
        db.execute("UPDATE shards SET tags = '[]' WHERE tags != '[]'")


def housekeep(db):
    t = now()
    db.execute("DELETE FROM sessions WHERE expires_at < ?", (t,))
    db.execute("DELETE FROM tokens WHERE expires_at < ?", (t,))
    db.execute("DELETE FROM oauth WHERE expires_at < ?", (t,))
    db.execute("DELETE FROM hits WHERE day < ?", (t // 86400 - 1,))


class tx:
    """BEGIN IMMEDIATE ... COMMIT (ROLLBACK on an exception)"""

    def __init__(self, db):
        self.db = db

    def __enter__(self):
        self.db.execute("BEGIN IMMEDIATE")
        return self.db

    def __exit__(self, kind, *_):
        self.db.execute("ROLLBACK" if kind else "COMMIT")


def new_id(db, table, n=6):
    while True:
        i = secrets.token_urlsafe(n)
        if not db.execute(f"SELECT 1 FROM {table} WHERE id = ?", (i,)).fetchone():
            return i
