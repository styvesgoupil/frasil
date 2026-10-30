#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""Announce a FRASIL release on the Discord server's #announcements channel.

  tools/discord_announce.py VERSION [--post] [--channel ID] [--summary FILE]

Without --post it is a dry run: it builds the message from CHANGELOG.md, prints the JSON payload, the target
channel and the length checks, and posts nothing. With --post it sends the message, publishes it to the channels
following #announcements (a News channel), and prints the message's URL.

The bot token is read from ~/.config/frasil/discord-bot.token (or $FRASIL_DISCORD_TOKEN_FILE), kept in a variable,
and sent only in the Authorization header. Standard library only.
"""
import argparse, json, os, re, sys, urllib.error, urllib.request

GUILD = "1563986548209356830"
API = "https://discord.com/api/v10"
UA = "DiscordBot (https://frasilsynth.com, 1.0)"
COLOUR = 0x6ED0FF
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOKEN_FILE = os.environ.get("FRASIL_DISCORD_TOKEN_FILE", os.path.expanduser("~/.config/frasil/discord-bot.token"))

DESC_MAX, EMBED_MAX, CONTENT_MAX, TITLE_MAX = 4096, 6000, 2000, 256
DESC_BUDGET = 3900          # what the built description aims under, a margin below Discord's 4096


def die(msg, code=1):
    print("discord_announce: " + msg, file=sys.stderr)
    sys.exit(code)


# ------------------------------------------------------------------ site.toml
def site_settings():
    out = {}
    try:
        text = open(os.path.join(ROOT, "website", "site.toml"), encoding="utf-8").read()
    except OSError:
        return out
    for m in re.finditer(r'^([a-z_]+)\s*=\s*"([^"]*)"', text, re.M):
        out.setdefault(m.group(1), m.group(2))
    return out


# ------------------------------------------------------------------ the changelog
def version_section(version):
    lines = open(os.path.join(ROOT, "CHANGELOG.md"), encoding="utf-8").read().split("\n")
    start = None
    for i, l in enumerate(lines):
        if re.match(r"^## " + re.escape(version) + r"( |$)", l):
            start = i + 1
            break
    if start is None:
        die("no '## %s' section in CHANGELOG.md" % version)
    body = []
    for l in lines[start:]:
        if l.startswith("## "):
            break
        body.append(l)
    return body


def plain(s):
    s = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", s)      # links: keep the text
    s = s.replace("`", "")
    return re.sub(r"\s+", " ", s).strip()


def parse(body):
    """-> (intro, [(heading, [item text])]). Headings are lines that are a lone **bold** label."""
    intro, sections, cur, item = [], [], None, None
    para = []

    def flush_item():
        nonlocal item
        if item is not None and cur is not None:
            cur[1].append(item)
        item = None

    for l in body:
        m = re.match(r"^\*\*([^*]+)\*\*\s*$", l)
        if m:
            flush_item()
            cur = (m.group(1).strip(), [])
            sections.append(cur)
        elif re.match(r"^- ", l):
            flush_item()
            if cur is None:                      # bullets with no heading (a point release)
                cur = ("Changes", [])
                sections.append(cur)
                if para:
                    intro.append(" ".join(para))
                    para = []
            item = l[2:].strip()
        elif l.startswith("  ") and item is not None:
            item += " " + l.strip()
        elif not l.strip():
            flush_item()
            if cur is None and para:
                intro.append(" ".join(para))
                para = []
        else:
            if cur is None:
                para.append(l.strip())
    if cur is None and para:
        intro.append(" ".join(para))
    flush_item()
    return [plain(p) for p in intro], sections


def lead(item, limit=70):
    """The bold lead; else the first short clause; else '' (the item is left out rather than cut mid-phrase)."""
    m = re.match(r"^\*\*(.+?)\*\*", item)
    if m:
        return plain(m.group(1).replace("**", "")).rstrip(" .:;,")
    t = plain(item).replace("**", "")
    m = re.match(r"^(.+?)(?:[.:;]\s|[.:;]$)", t)
    if m:
        t = m.group(1)
    if len(t) > limit or t.count("(") != t.count(")"):
        return ""
    return t.strip().rstrip(" .:;,")


def pick_sections(sections):
    """New / Fixes and changes, or Highlights / Changes where a version has those instead."""
    by = {h.lower(): items for h, items in sections}
    out = []
    first = by.get("new") or by.get("highlights")
    if first:
        out.append(("Highlights" if "new" not in by else "New", first))
    for key in ("fixes and changes", "changes"):
        if by.get(key):
            out.append(("Fixes and changes", by[key]))
    return out


# ------------------------------------------------------------------ the message
def links(version, changelog_only_more=False):
    s = site_settings()
    repo = s.get("repo", "https://github.com/styvesgoupil/frasil").rstrip("/")
    site = s.get("site_url", "https://frasilsynth.com/")
    if not site.endswith("/"):
        site += "/"
    return {
        "release": "%s/releases/tag/v%s" % (repo, version),
        "site": site,
        "notes": "%schangelog/#v%s" % (site, version.replace(".", "-")),
        "install": site + "guide/install/",
        "river": s.get("river_url", "https://river.frasilsynth.com/"),
        "perform": "https://perform.frasilsynth.com/",
    }


def links_line(version, mentions, L):
    parts = ["[Release](%s)" % L["release"], "[Full notes](%s)" % L["notes"], "[Install guide](%s)" % L["install"]]
    if re.search(r"\bRiver\b", mentions):
        parts.append("[River](%s)" % L["river"])
    if re.search(r"\bPERFORM\b|perform\.frasilsynth", mentions):
        parts.append("[Perform](%s)" % L["perform"])
    parts.append("[frasilsynth.com](%s)" % L["site"])
    return " · ".join(parts)


def build_description(version, summary_file=None):
    L = links(version)
    body = version_section(version)
    mentions = "\n".join(body)
    tail = "\n\n" + links_line(version, mentions, L)
    if summary_file:
        text = open(summary_file, encoding="utf-8").read().strip()
        return text + tail, L, False
    intro, sections = parse(body)
    groups = [(h, [lead(i) for i in items]) for h, items in pick_sections(sections)]
    groups = [(h, [x for x in ls if x]) for h, ls in groups]
    counts = [len(ls) for _, ls in groups]
    total = list(counts)
    more_line = "\n…and more: [full notes](%s)" % L["notes"]

    def render(counts):
        cut = counts != total
        parts = []
        if intro:
            parts.append("\n\n".join(intro))
        for (h, ls), n in zip(groups, counts):
            if n:
                parts.append("**%s**\n%s" % (h, "\n".join("- " + x for x in ls[:n])))
        text = "\n\n".join(parts)
        if cut:
            text += "\n" + more_line
        return text + tail, cut

    while True:
        text, cut = render(counts)
        if len(text) <= DESC_BUDGET or not any(counts):
            return text, L, cut
        k = max(range(len(counts)), key=lambda j: counts[j])      # drop from the longer list's end
        counts[k] -= 1


# ------------------------------------------------------------------ Discord
def read_token():
    try:
        tok = open(TOKEN_FILE, encoding="utf-8").read().strip()
    except OSError:
        return None
    return tok or None


def api(method, path, token, body=None):
    data = None if body is None else json.dumps(body).encode("utf-8")
    req = urllib.request.Request(API + path, data=data, method=method)
    req.add_header("Authorization", "Bot " + token)
    req.add_header("User-Agent", UA)
    if data is not None:
        req.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(req, timeout=20) as r:
            raw = r.read()
            return json.loads(raw) if raw else {}
    except urllib.error.HTTPError as e:
        try:
            detail = e.read().decode("utf-8", "replace")[:600]
        except Exception:
            detail = ""
        raise RuntimeError("%s %s -> HTTP %d %s" % (method, path, e.code, detail))
    except urllib.error.URLError as e:
        raise RuntimeError("%s %s -> %s" % (method, path, e.reason))


def find_channel(token, channel_id):
    if channel_id:
        c = api("GET", "/channels/%s" % channel_id, token)
        return c["id"], c.get("name"), c.get("type")
    for c in api("GET", "/guilds/%s/channels" % GUILD, token):
        if c.get("name") == "announcements" and c.get("type") == 5:
            return c["id"], c["name"], c["type"]
    raise RuntimeError("no news channel named 'announcements' in the guild (use --channel ID)")


def main():
    ap = argparse.ArgumentParser(description="Announce a FRASIL release on Discord (dry run unless --post).")
    ap.add_argument("version")
    ap.add_argument("--post", action="store_true", help="send it and publish it (default: dry run)")
    ap.add_argument("--channel", metavar="ID", help="channel id (default: #announcements, looked up)")
    ap.add_argument("--summary", metavar="FILE", help="a Markdown file as the description instead of the changelog's")
    a = ap.parse_args()
    if not re.fullmatch(r"\d+\.\d+(\.\d+)?", a.version):
        die("VERSION looks like 2.5 or 2.0.1")

    desc, L, cut = build_description(a.version, a.summary)
    embed = {"title": "FRASIL %s is out" % a.version, "url": L["release"], "description": desc, "color": COLOUR}
    payload = {"embeds": [embed], "allowed_mentions": {"parse": []}}

    embed_total = len(embed["title"]) + len(desc)
    checks = [("description", len(desc), DESC_MAX), ("embed total", embed_total, EMBED_MAX), ("content", 0, CONTENT_MAX),
              ("title", len(embed["title"]), TITLE_MAX)]
    bad = [n for n, v, m in checks if v > m]

    token = read_token()
    chan = None
    if not token:
        print("(no token at %s: channel not looked up)" % TOKEN_FILE, file=sys.stderr)
        if a.post:
            die("no bot token to post with")
    else:
        try:
            chan = find_channel(token, a.channel)
        except RuntimeError as e:
            if a.post:
                die(str(e))
            print("(channel lookup failed: %s)" % e, file=sys.stderr)

    print(json.dumps(payload, indent=2, ensure_ascii=False))
    print()
    print("target   %s" % ("#%s  id %s  type %s%s" % (chan[1], chan[0], chan[2], " (news)" if chan[2] == 5 else "")
                            if chan else (a.channel or "(unknown)")))
    for n, v, m in checks:
        print("%-12s %5d / %d  %s" % (n, v, m, "OK" if v <= m else "TOO LONG"))
    if cut:
        print("note     bullets were dropped to fit; the message ends with 'and more: full notes'")
    if bad:
        die("over Discord's limits: " + ", ".join(bad))
    if not a.post:
        print("\ndry run: nothing posted. Add --post once the GitHub release is live.")
        return

    cid = chan[0]
    try:
        msg = api("POST", "/channels/%s/messages" % cid, token, payload)
    except RuntimeError as e:
        die("post failed: %s" % e)
    url = "https://discord.com/channels/%s/%s/%s" % (GUILD, cid, msg["id"])
    if chan[2] == 5:
        try:
            api("POST", "/channels/%s/messages/%s/crosspost" % (cid, msg["id"]), token)
            print("published to following servers")
        except RuntimeError as e:
            print("warning: crosspost failed: %s" % e, file=sys.stderr)
    print(url)


if __name__ == "__main__":
    main()
