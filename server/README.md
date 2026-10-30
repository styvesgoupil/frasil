# FRASIL River's service

The API behind **https://river.frasilsynth.com/**: accounts (email + password, or Continue with Google), each user's
library of **shards** (FRASIL scenes, as the FM-1 keeps them) and **backups** (the FM-1's `.frasil` files), and the
public River (browse, sort, filter, play, send, favorite, report). Python 3.12, FastAPI, SQLite (WAL) in one Docker
container on nimbus; the host's Caddy serves the page and passes `/api/*` here, on the same origin.

| File | What |
| --- | --- |
| `app.py` | the app (`create_app`), the health route |
| `auth.py` | accounts, sessions, email verification and password reset, Google, `/api/me` |
| `shards.py` | my shards, favorites and backups, the public River, counters, save, report, moderation, the export |
| `scenes.py` | what is accepted: shards and backups, read with the schema |
| `waitlist.py` | the waitlists (FRASIL for iPad, FRASIL VST): an email left to be told when it is out |
| `admin.py` | the admin page's routes (admin.frasilsynth.com): accounts, uploads, waitlists, behind `FRASIL_ADMIN_KEY` |
| `guard.py` | rate limits, the word filter, same-origin JSON for changes, cleaning what users type |
| `mail.py` | the emails (SMTP over TLS; with no SMTP, the log) |
| `store.py` | the settings (from the environment) and the database |
| `make_schema.py` | writes `schema.json` from the simulator (see below) |
| `Dockerfile`, `docker-compose.yml`, `.env.example`, `deploy.sh`, `backup.sh` | running it on nimbus |
| `tests/` | pytest, FastAPI's TestClient |

## The schema: never a layout copied by hand

Every shard the service receives is checked with **the schema the firmware's own code writes**: the simulator's
`schema FILE` (`app.c` `schema_write`), the same one the River's page reads (`website/assemble.py`). `make_schema.py`
runs the simulator and writes `server/schema.json`, adding the backups' magic and version from `app.h`
(`APP_BACKUP_MAGIC`, `APP_BACKUP_VERSION`) and FRASIL's `VERSION`. It is not committed (`.gitignore`): `deploy.sh`
and the tests make it, and the Docker build copies it (the build stops without it).

    ./build.sh --host                 # the simulator, app/build/host/frasil_host
    python3 server/make_schema.py     # -> server/schema.json

A shard is accepted when it is the current layout's `scene_t` (`FRSk`: 272 bytes, the bass's sequence last), every
sound setting is at most its `max`, and its name (16 bytes) is 1 to 16 of `A-Z 0-9 space - . # +`, then NULs. What
the FM-1 may leave after the name's NUL (an older name's letters) is cleared. A backup must be fm1-base's format (as
`base/tools/fm1_backup.py` `parse`): FRASIL's magic `FRBK` and version 1, every part's CRC-32 and the whole file's;
8 KB at most. When FRASIL's layout changes, run `make_schema.py` again. Shards of the earlier layouts the schema lists
(`old_shards`: `FRSi`, before BASS's DECAY, SUSTAIN and PULSE; `FRSh`, before the bass's sequence too; and `FRSg`,
names of ten) are made over into the current one as the FM-1 would (`scenes.Schema.upgraded`, step by step: the new
settings at their defaults, the bass empty, the name NUL-padded): those kept, at startup (once), and those a page
still open from before sends.

## Run it here

    cd server
    python3 -m venv .venv && .venv/bin/pip install -r requirements-dev.txt
    python3 make_schema.py
    .venv/bin/python -m pytest -q
    ../tools/website.sh                # the page: build/website/river
    RIVER_URL=http://localhost:8050 COOKIE_SECURE=0 RIVER_STATIC=../build/website/river RIVER_DB=dev.sqlite \
        .venv/bin/uvicorn app:create_app --factory --port 8050 --reload

Then http://localhost:8050/ is the River with its API at `/api/`, on one origin as on nimbus. With no `SMTP_HOST`,
the emails (their links) are written to the log.

## Settings

All from the environment (`.env` on nimbus, from `.env.example`; `store.Settings`):

| Variable | Default | What |
| --- | --- | --- |
| `RIVER_URL` | `https://river.frasilsynth.com` | the site's origin: links in emails; the only `Origin` a change is accepted from |
| `RIVER_DB` | `river.sqlite` (`/data/river.sqlite` in Docker) | the database |
| `RIVER_SCHEMA` | `schema.json` beside the code | the schema |
| `SMTP_HOST`, `SMTP_PORT` | none, `465` | Zoho: `smtppro.zoho.com` (an organization mailbox; `smtp.zoho.com` a personal one), 465 (TLS); 587 is STARTTLS. None: the log |
| `SMTP_USER`, `SMTP_PASSWORD` | | the Zoho login (the mailbox noreply@ is an alias of) and its app password |
| `MAIL_FROM` | `FRASIL River <noreply@frasilsynth.com>` | the sender |
| `GOOGLE_CLIENT_ID`, `GOOGLE_CLIENT_SECRET` | none | Continue with Google (none: off, `/api/auth/google/start` is 404) |
| `GOOGLE_REDIRECT_URI` | `$RIVER_URL/api/auth/google/callback` | as registered with Google |
| `ADMIN_EMAILS` | none | comma-separated; an admin's email must also be verified on the River |
| `WORD_FILTER` | `guard.WORDS` | comma-separated words replacing the default list |
| `CLIENT_IP_HEADER` | `cf-connecting-ip` | where the client's address is (Cloudflare's, through Caddy); empty: the connection |
| `COOKIE_SECURE` | `1` | `0` only for a local run over plain http |
| `SITE_URL` | `https://frasilsynth.com` | the main site's origin: `POST /api/waitlist` is accepted from it too |
| `FRASIL_ADMIN_KEY` | none | the admin page's key, in the header `X-Frasil-Admin` (none: `/api/admin/overview`, `users/{id}`, `waitlist` are 404) |
| `RIVER_STATIC` | none | a folder served at `/` (a local run only) |
| `TZ` | `America/Toronto` | the container's |

## The API

JSON, under `/api/`. Errors: a status and `{"detail": "a sentence for the page"}` (401 not signed in, 403 not
allowed, 404, 409 conflict, 413 too big, 415 not JSON, 422 not valid, 429 too many: with `Retry-After`).

**Every change** (POST, PUT, PATCH, DELETE) must carry `Origin: <RIVER_URL>` and, but for DELETE, `Content-Type:
application/json` (a body-less POST sends `{}` or nothing): with the session's cookie being `SameSite=Lax`, no other
site can make one. Requests are 64 KB at most.

| Route | What |
| --- | --- |
| `POST /api/auth/register {email, password, display_name}` | 201, signed in; a verification link is emailed |
| `POST /api/auth/login {email, password}` | signed in (401 wrong, 403 banned) |
| `POST /api/auth/logout {all?}` | `all: true`: every session of the account |
| `GET /api/auth/session` | `{me (the account, or null), password, google}`: the page's first question, never a 401 |
| `GET /api/auth/providers` | `{password, google}`: Continue with Google only when it is set up |
| `GET /api/auth/google/start?next=/path` | to Google, then back to `next` (a path on the River) |
| `GET /api/auth/google/callback` | Google's redirect; on failure to `/#google=failed` or `/#google=banned` |
| `POST /api/auth/verify {token}` | the email verified (signed in as the account it was sent for) |
| `POST /api/auth/resend` | a new verification link |
| `POST /api/auth/forgot {email}` | always `{ok: true}`; a reset link to an account's email |
| `POST /api/auth/reset {token, password}` | a new password: other sessions end, signed in, the email verified |
| `GET /api/me` · `PATCH /api/me {display_name?, password?, new_password?}` · `DELETE /api/me` | the account (changing the password: the current one in `password`) |
| `GET /api/me/export` | a zip: `account.json`, `backups/*.frasil`, `shards-FRSg.frasilscenes` (the page's package, `FB.readPack`) |
| `GET /api/shards` · `POST /api/shards {name?, layout, firmware, data, from_slot?, description?, origin?}` | my library (`data`: the `scene_t` in base64; `from_slot`: 0-15, the page's `scene.slot`; `origin`: the shard the editor's SAVE AS made it from: someone else's public one is credited, CC BY 4.0, as a copy is; one of mine passes its credit on) |
| `PATCH /api/shards/{id} {name?, description?, data?, layout?, firmware?}` · `DELETE /api/shards/{id}` | a new name is written into `data` (upper-cased); `data` (the editor's SAVE: the current `layout` only) replaces its sound, checked as a new shard's, its name kept |
| `POST /api/shards/{id}/share {name?, description?}` · `POST /api/shards/{id}/unshare` | public under CC BY 4.0 (needs a verified email; 50 a day) |
| `GET /api/favorites` | `{items}`: my favorites, the newest favorited first (below) |
| `POST /api/favorites/{id}` · `DELETE /api/favorites/{id}` | `{favorited, favorites}`: a shard of the River bookmarked (201; one already: 200; my own: 409; not in the River: 404), or not any more (one no longer shared too; not one: 200 all the same) |
| `GET /api/backups` · `POST /api/backups {image, note?, firmware?}` · `GET /api/backups/{id}` · `DELETE /api/backups/{id}` | my backups (the list without `image`; each with `scenes`: the names saved, slot by slot) |
| `GET /api/public/shards?sort=new\|played\|favorites\|popular&q=&root=&scale=&type=&drone=&bass=seq\|none&page=` | `{items, page, pages, total, sort}`, 24 a page |
| `GET /api/public/shards/{id}` | one public shard |
| `POST /api/public/shards/{id}/played` · `…/sent` | counted once a day per listener (a user, or a hashed IP) |
| `POST /api/public/shards/{id}/save` | v1's SAVE, until the River v2 ships: a copy into my library (signed in), its first author credited |
| `POST /api/public/shards/{id}/report {reason}` | a report (once per reporter) |
| `GET /api/admin/reports?all=` · `POST /api/admin/reports/{id}/resolve` | the admins' |
| `POST /api/admin/shards/{id}/hide\|unhide\|delete` · `POST /api/admin/users/{id}/ban\|unban` | the admins' |
| `POST /api/waitlist {list: ipad\|vst, email}` | `{ok: true}`; once per list and email (again: the same, nothing added); no email sent; 20 / h per IP; from the River or `SITE_URL` |
| `GET /api/admin/overview` · `GET /api/admin/users/{id}` · `GET /api/admin/waitlist` | the admin page's, read-only: `X-Frasil-Admin: $FRASIL_ADMIN_KEY` or 404 (`admin.py`) |
| `GET /api/health` | `{ok, layout, firmware}` |

A shard: `{id, name, description, layout, firmware, data, from_slot, root, scale, type, drone, glint, bseq,
visibility, license, origin_id, origin_author, created_at, updated_at, shared_at, plays, sends, saves, favorites}`; a
public one adds `author` and `favorited` (among the signed-in user's favorites; false signed out) and leaves out
`from_slot`, `visibility`, `updated_at`; a favorite is a public one with `favorited_at` and `shared` (false: its
author took it out of the River since; it keeps its data, to play and send to the FM-1, but isn't counted then). One
deleted is gone from the favorites; one hidden by the moderators, or whose author is banned, is left out of them
(back when that is undone). `root`, `scale`, `type` (GLINT's OFF, ARP, SEQ), `drone` and `glint` (their presets) are
the setting's number, read from `data` at the schema's offsets; the filters take that number or the word the FM-1
shows (`root=D#`, `scale=minor`, `type=arp`). `bseq`: 1 when the bass's sequence is on (`bq_on`; the list's BASS:
SEQ), else 0; `bass=seq` or `bass=none`. `q`: words, each found in the name or the description. The sorts: `new` (the
default: the newest shared first), `played`, `favorites` (the most first), `popular`: `(1 + plays + 3 sends + 5 kept)
/ (days since shared + 2)^1.5`, `kept` its saves (v1's copies, each user once) and its favorites from users who hadn't
saved it. `saves` stays, frozen when v2 drops SAVE; a shard has no title nor tags any more (the requests' are
ignored: v1's page still sends them; at startup, each title became its description's first sentence).

**The page's links**: the verification email links to `RIVER_URL/#verify=TOKEN`, the reset email to
`RIVER_URL/#reset=TOKEN` (in the fragment: the token never reaches a server's log); the page posts the token to
`/api/auth/verify` or asks for a new password and posts `/api/auth/reset`. A verification counts only from a session
of the account it was sent for (else 401 or 400: the page asks to sign in, then posts it again): whoever opens the
link can't verify, for its maker, an account someone else made with their email. The emails carry nothing a user
typed (they reach addresses not yet proved). The page must show what users type (names, descriptions, display
names) as text (`textContent`), never as HTML.

## Security, in short

- Passwords: argon2id (argon2-cffi's defaults: 64 MiB each, 4 at a time at most; the container capped at 512 MB),
  rehashed when those change; a login for no one takes as long.
- Sessions: a random 256-bit token in the cookie `river_session` (`HttpOnly; Secure; SameSite=Lax; Path=/api`), only
  its SHA-256 kept; 90 days, renewed as it is used (once a day at most); signing in ends the browser's previous one.
- One-time links (their SHA-256 kept): verification 48 h, reset 1 h (a new one replaces the last).
- Google: the authorization code flow with PKCE (S256), a `state` bound to the browser by a 10-minute cookie, a
  `nonce`; the ID token comes straight from Google's token endpoint over TLS, so its claims are checked (issuer,
  audience, expiry, nonce, `email_verified`) without its signature (OpenID Connect Core 3.1.3.7). The account is
  the one with that email; one that was made with it but never verified loses its password and sessions (its maker
  didn't own the email).
- Rate limits, in the process's memory (one worker): login 20 / 15 min per IP, and failures 10 / 15 min per email
  from one IP and 50 / h per email (only failures: no one locks the owner out); register 10 / h per IP; forgot 5 / h per IP and 3 per email; links 30 / h per IP; resend 3 / h; uploads 200 shards and 30 backups / h
  per account; shares 50 / day; saves 100 / h; favorites 200 / h; reports 10 / h per IP; counters 300 / h per IP.
- The client's address is Cloudflare's `CF-Connecting-IP`: only true of requests that came through Cloudflare, so
  Caddy refuses `/api/` to any other peer (below). Without that, anyone reaching nimbus directly could choose their
  address and dodge every limit by IP.
- Kept about a person: their email, display name and what they upload. IPs only salted and hashed (counters,
  reports). `DELETE /api/me` deletes all of it (and others' favorites of its shards); copies others saved stay theirs, without the author's name (CC BY
  4.0, 3(a)(3)).
- A banned user can't sign in (their sessions end) and their shards leave the River (kept, should the ban be lifted).
  An admin can't be banned. Hidden shards stay with their owner, who can't share them again.

## Deploy (on Styves's go)

The first time, on nimbus: `mkdir -p ~/apps/frasil`, copy `.env.example` to `~/apps/frasil/.env` and fill it in
(`chmod 600`). Then, from the Mac:

    server/deploy.sh          # make_schema.py, the tests, rsync to ~/apps/frasil, docker compose up -d --build, health

The container is `frasil-api`, on `127.0.0.1:3050`, its data in the volume `frasil_frasil-data` (`/data/river.sqlite`).

**Caddy** (`/etc/caddy/Caddyfile`, the `river.frasilsynth.com` block). Back it up first
(`Caddyfile.bak-YYYYMMDD-HHMMSS-before-river-api`), then `sudo caddy validate --config /etc/caddy/Caddyfile`, `sudo
systemctl reload caddy`, and check the other sites still answer. The block becomes:

    river.frasilsynth.com {
    	tls /etc/caddy/certs/frasilsynth.com.pem /etc/caddy/certs/frasilsynth.com.key
    	encode gzip zstd
    	header Strict-Transport-Security "max-age=31536000"
    	header Content-Security-Policy "frame-ancestors 'none'"
    	header X-Frame-Options "DENY"
    	handle /api/* {
    		@direct not remote_ip 173.245.48.0/20 103.21.244.0/22 103.22.200.0/22 103.31.4.0/22 141.101.64.0/18 108.162.192.0/18 190.93.240.0/20 188.114.96.0/20 197.234.240.0/22 198.41.128.0/17 162.158.0.0/15 104.16.0.0/13 104.24.0.0/14 172.64.0.0/13 131.0.72.0/22 2400:cb00::/32 2606:4700::/32 2803:f800::/32 2405:b500::/32 2405:8100::/32 2a06:98c0::/29 2c0f:f248::/32
    		abort @direct
    		request_body {
    			max_size 64KB
    		}
    		reverse_proxy 127.0.0.1:3050
    	}
    	handle {
    		root * /var/www/river.frasilsynth.com
    		file_server
    	}
    	handle_errors 404 {
    		redir https://river.frasilsynth.com/
    	}
    }

`@direct`: Cloudflare's ranges (https://www.cloudflare.com/ips/: check them against that page when applying, and
when Cloudflare changes them). The API answers only requests that came through Cloudflare, whose `CF-Connecting-IP`
is then the client's (nimbus's firewall lets anyone reach 443). The two headers keep the page out of other sites'
frames (no clickjacking of Delete or Share).

**DNS**: `river.frasilsynth.com` already points to nimbus (A, proxied): nothing to change for the API. For the
emails, the `frasilsynth.com` zone has no mail records yet (2026-10-23); add, from Zoho's admin console (Domains):
its verification TXT at `@`; MX `mx.zoho.com` 10, `mx2.zoho.com` 20, `mx3.zoho.com` 50 (for bounces and replies to
reach Zoho); SPF, a TXT at `@`: `v=spf1 include:zohomail.com ~all`; Zoho's DKIM key, a TXT at
`<selector>._domainkey` (the selector and key Zoho shows); DMARC, a TXT at `_dmarc`:
`v=DMARC1; p=none; rua=mailto:<an address of Styves's>`.

**Google**: in Google Cloud's console, an OAuth client of type *Web application*, its redirect URI
`https://river.frasilsynth.com/api/auth/google/callback`; the consent screen with the scopes `openid`, `email`,
`profile` (no verification needed for those), published (not "Testing", or only test users can sign in).

**Backups**: `backup.sh` (deployed with the rest) takes SQLite's online backup (as `sqlite3 .backup`) inside the
container each night, gzipped into `~/apps/backups/frasil` (30 kept). The database holds emails and password hashes,
so the off-host copy goes only to an encrypted remote: once, on nimbus, `rclone config` → new remote `gdrive-crypt`,
type `crypt`, remote `gdrive:frasil-crypt`, a password (and salt) Styves keeps somewhere safe besides nimbus; until
then the off-host copy is skipped (logged in `backup.log`). Add it to the
crontab once: `15 4 * * * /home/deploy/apps/frasil/backup.sh`. Or a copy by hand, on nimbus, with the sqlite3 CLI
(the same online backup, the service running):

    sudo sqlite3 /var/lib/docker/volumes/frasil_frasil-data/_data/river.sqlite ".backup '/home/deploy/river-$(date +%F).db'"
