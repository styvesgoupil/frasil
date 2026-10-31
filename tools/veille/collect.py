#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""The veille's gathering, no judgement in it (run.sh, on the M3): YouTube through its API (searches, the channels that
cover the FM-1, descriptions, comments), Reddit through its feeds, what is said in videos through their subtitles
(yt-dlp, a few each pass). Writes WORK/candidates.json: `auto`, ready to send (what is known brought up to date, the owner's
own replies), `candidates`, for the agent to judge (prompt.md); their words as found, never rewritten.
    ~/.config/frasil/veille.token, youtube.key     ~/.local/state/frasil-veille/ (its memory, its files)"""
import glob
import html
import json
import os
import re
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request

CONF = os.path.expanduser("~/.config/frasil")
WORK = os.path.expanduser("~/.local/state/frasil-veille")
API = "https://admin.frasilsynth.com/api/veille"
WATCH = "https://www.youtube.com/watch?v="
THREAD = "https://www.reddit.com/r/mvavefm1/comments/"
UA = "Mozilla/5.0 (Macintosh; Intel Mac OS X 14_0) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Safari/605.1.15"
SAID = re.compile(r"fr[ae]s[ie]l", re.I)                       # (written)
HEARD = re.compile(r"\b(fr[aeiu]s+[aeiou]+l+\w*|fr[aeiu]z+[aeiou]*l+\w*|frasier|fraser|frazier|freezel|fresil)\b", re.I)    # (as subtitles hear it)
FM1 = re.compile(r"fm.?1|m.?vave", re.I)
OWN = re.compile(r"styvesgoupil", re.I)
EXACT = ("frasil fm-1", "frasil firmware", "frasil synth")
BROAD = ("M-VAVE FM-1 firmware", "FM-1 firmware hub", "M-Vave FM-1")
KEEP_DAYS, SUBS_PER_PASS, COMMENT_PAGES, QUOTE_MAX = 21, 12, 6, 2000
notes = []


def secret(name):
    return open(os.path.join(CONF, name)).read().strip()


def get(url, headers=None, tries=3):
    for n in range(tries):
        try:
            r = urllib.request.Request(url, headers={"User-Agent": UA, **(headers or {})})
            return urllib.request.urlopen(r, timeout=25).read().decode("utf8", "replace")
        except urllib.error.HTTPError as e:
            if e.code == 429 and n + 1 < tries:
                time.sleep(15 * (n + 1))
                continue
            return e.code
        except OSError:
            time.sleep(5)
    return 0


KEY = None


def yt(endpoint, **params):
    s = get(f"https://www.googleapis.com/youtube/v3/{endpoint}?{urllib.parse.urlencode(params)}", {"X-Goog-Api-Key": KEY})
    if not isinstance(s, str):
        return {"error": s}
    return json.loads(s)


def iso(t):
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(t))


def youtube(state, by_url):
    videos, cut = state.setdefault("videos", {}), iso(time.time() - KEEP_DAYS * 86400)
    found, since = {}, iso(time.time() - 3 * 86400)
    for q, after, order in [(q, "2026-10-01T00:00:00Z", "relevance") for q in EXACT] + [(q, since, "date") for q in BROAD]:
        d = yt("search", part="id", q=q, type="video", maxResults=50, publishedAfter=after, order=order)
        if "error" in d:
            notes.append(f"YouTube search {d['error']}")
            return [], []
        found.update({x["id"]["videoId"]: 1 for x in d["items"]})
    for c in state.setdefault("channels", [])[:40]:
        d = yt("playlistItems", part="snippet", playlistId="UU" + c[2:], maxResults=15)
        found.update({x["snippet"]["resourceId"]["videoId"]: 1 for x in d.get("items", []) if x["snippet"]["publishedAt"] >= cut})
    for u in list(by_url) + [k["parent_url"] for k in by_url.values() if k.get("parent_url")]:
        m = re.match(re.escape(WATCH) + r"([\w-]+)$", u)
        if m:
            found[m[1]] = 1
    ids = list(dict.fromkeys(list(videos) + list(found)))
    cands, auto, chans = [], [], {}
    for i in range(0, len(ids), 50):
        for v in yt("videos", part="snippet,statistics", id=",".join(ids[i:i + 50])).get("items", []):
            s, vid, url = v["snippet"], v["id"], WATCH + v["id"]
            n = int(v["statistics"].get("commentCount", 0) or 0)
            text = s["title"] + " " + s["description"]
            about = bool(FM1.search(text)) or bool(SAID.search(s["title"]))
            if s["publishedAt"] < cut and url not in by_url or not about:
                videos.pop(vid, None)
                continue
            chans[s["channelId"]] = chans.get(s["channelId"], 0) + 1
            was = videos.setdefault(vid, {})
            was.update(title=s["title"], channel=s["channelTitle"], at=s["publishedAt"], views=int(v["statistics"].get("viewCount", 0)))
            k = by_url.get(url)
            if k and n != k.get("engagement"):
                auto.append({**base(k), "engagement": n})
            lines = [x.strip() for x in s["description"].split("\n") if SAID.search(x)]
            if not k and (lines or SAID.search(s["title"])):
                cands.append(dict(url=url, platform="youtube", kind="video", title=s["title"], author=s["channelTitle"], community=s["channelTitle"],
                                  published_at=s["publishedAt"], engagement=n, quote="\n".join(lines)[:QUOTE_MAX] or None, language=(s.get("defaultAudioLanguage") or "en")[:2]))
            if n and was.get("comments") != n:
                c, a = comments(vid, s["title"], s["channelTitle"], by_url)
                cands += c
                auto += a
                was["comments"] = n
    state["channels"] = [c for c, n in sorted(chans.items(), key=lambda x: -x[1]) if n > 1] or state["channels"]
    return cands, auto


def base(k):
    return {x: k[x] for x in ("url", "platform", "kind", "title", "summary", "language", "sentiment", "priority", "parent_url")}


def comments(vid, title, channel, by_url):
    cands, auto, tok = [], [], None
    for _ in range(COMMENT_PAGES):
        p = dict(part="snippet,replies", videoId=vid, maxResults=100, textFormat="plainText")
        if tok:
            p["pageToken"] = tok
        d = yt("commentThreads", **p)
        if "error" in d:
            break
        for th in d["items"]:
            for c in [th["snippet"]["topLevelComment"]] + th.get("replies", {}).get("comments", []):
                t, who, url = c["snippet"].get("textDisplay") or "", c["snippet"].get("authorDisplayName") or "", f"{WATCH}{vid}&lc={c['id']}"
                if url in by_url:
                    continue
                item = dict(url=url, platform="youtube", kind="comment", title=f"Commentaire sous « {title[:90]} »", author=who, community=channel,
                            parent_url=WATCH + vid, parent_title=title, published_at=c["snippet"]["publishedAt"], quote=t[:QUOTE_MAX], language="en")
                if OWN.search(who):
                    auto.append(mine(item))
                elif SAID.search(t):
                    cands.append(item)
        tok = d.get("nextPageToken")
        if not tok:
            break
    return cands, auto


def mine(item):
    return {**item, "title": item["title"].replace("Commentaire sous", "Réponse sous"), "summary": "Ta réponse.", "sentiment": "neutral", "priority": 1, "is_own": True}


def seconds(t):
    h, m, s = t.split(":")
    return int(h) * 3600 + int(m) * 60 + int(float(s))


def spoken(vid):
    """where a video's subtitles say it: [(seconds, words)]; None: couldn't read them (the limit's reached), [] nothing"""
    with tempfile.TemporaryDirectory() as d:
        try:
            r = subprocess.run(["yt-dlp", "--skip-download", "--write-auto-subs", "--write-subs", "--sub-langs", "en,en-orig,fr,fr-orig", "--sub-format", "vtt",
                                "--retries", "1", "--extractor-retries", "1", "--no-warnings", "-q", "-o", f"{d}/s", WATCH + vid],
                               capture_output=True, text=True, timeout=90)
        except subprocess.TimeoutExpired:
            return None
        files = glob.glob(d + "/*.vtt")
        if not files:
            return None if r.returncode or re.search("429|confirm you", r.stderr) else []
        out = {}
        cues = re.findall(r"(\d\d:\d\d:\d\d\.\d+) --> .*?\n(.*?)(?:\n\n|\Z)", open(files[0], errors="replace").read(), re.S)
        for i, (at, text) in enumerate(cues):
            text = re.sub(r"\s+", " ", re.sub(r"<[^>]+>", "", text)).strip()
            if HEARD.search(text):
                out.setdefault(seconds(at) // 20 * 20, re.sub(r"\s+", " ", re.sub(r"<[^>]+>", "", " ".join(c[1] for c in cues[max(0, i - 1):i + 2]))).strip())
        return sorted(out.items())


def transcripts(state, by_url):
    cands, auto, read = [], [], 0
    todo = sorted((x for x in state["videos"].items() if "subs" not in x[1]), key=lambda x: -x[1].get("views", 0))
    for vid, v in todo[:SUBS_PER_PASS]:
        hits = spoken(vid)
        if hits is None:
            notes.append(f"sous-titres limités après {read} vidéos ({len(todo) - read} en attente)")
            break
        read, v["subs"] = read + 1, True
        if hits:
            moments = [dict(at=a, quote=q[:300]) for a, q in hits[:20]]
            k = by_url.get(WATCH + vid)
            item = dict(url=WATCH + vid, platform="youtube", kind="video", title=v["title"], author=v["channel"], community=v["channel"], published_at=v["at"],
                        moments=moments, language="en", heard=True, already=bool(k))
            cands.append({**(base(k) if k else {}), **item})
        time.sleep(15)
    notes.append(f"sous-titres : {read} lus, {max(0, len(todo) - read)} en attente")
    return cands, auto


def text_of(raw):
    t = html.unescape(html.unescape(raw))
    t = re.sub(r"</p>|<br ?/?>", "\n", t)
    t = html.unescape(re.sub("<[^>]+>", "", t))
    return re.sub(r"\n{3,}", "\n\n", re.sub(r"[ \t]+", " ", re.split(r"\s*submitted by\s", t)[0])).strip()


def entries(s):
    for e in re.findall(r"<entry>(.*?)</entry>", s, re.S):
        who, link, at, body = (re.search(p, e, re.S) for p in (r"<name>/u/([^<]+)", r'<link href="([^"]+)"', r"<updated>([^<]+)", r"<content[^>]*>(.*?)</content>"))
        title = re.search(r"<title>(.*?)</title>", e, re.S)
        if link:
            yield dict(who=who and who[1], link=html.unescape(link[1]), at=at and at[1], body=text_of(body[1]) if body else "", title=html.unescape(title[1]) if title else "")


def reddit(state, by_url):
    cands, auto, threads = [], [], {}
    for q in ("r/MVaveFM1/search.rss?restrict_sr=on&q=frasil", "search.rss?q=frasil%20fm-1", "search.rss?q=frasil%20firmware"):
        s = get(f"https://www.reddit.com/{q}&sort=new&limit=50")
        if not isinstance(s, str):
            notes.append(f"Reddit {s} sur la recherche")
            continue
        for e in entries(s):
            m = re.search(r"/r/(\w+)/comments/(\w+)/", e["link"])
            if m and (FM1.search(e["title"] + e["body"]) or m[1].lower() == "mvavefm1"):
                threads[m[2]] = m[1]
        time.sleep(14)
    for u in by_url:
        m = re.search(r"/r/(\w+)/comments/(\w+)/", u)
        if m:
            threads.setdefault(m[2], m[1])
    for t, sub in threads.items():
        s = get(f"https://www.reddit.com/r/{sub}/comments/{t}/.rss?limit=100&sort=new")
        time.sleep(14)
        if not isinstance(s, str):
            notes.append(f"Reddit {s} sur {t}")
            continue
        es, group = list(entries(s)), f"https://www.reddit.com/r/{sub.lower()}/comments/{t}/"
        tt = re.search(r"<title>(.*?)</title>", s)
        title = html.unescape(tt[1]).split(" : ")[0] if tt else t
        root = by_url.get(group)
        if root and root.get("engagement") != len(es) - 1:
            auto.append({**base(root), "engagement": len(es) - 1})
        for e in es:
            cid = re.search(r"/comments/\w+/[^/]*/(\w+)/", e["link"])
            if not cid:
                if not root and SAID.search(e["title"] + e["body"]):
                    lines = [x for x in e["body"].split("\n") if SAID.search(x)] if len(e["body"]) > 900 else [e["body"]]
                    cands.append(dict(url=group, platform="reddit", kind="thread", title=title, author=e["who"], community="r/" + sub, published_at=e["at"],
                                      engagement=len(es) - 1, quote="\n".join(lines)[:QUOTE_MAX], language="en"))
                continue
            url = f"{group}comment/{cid[1]}/"
            if url in by_url:
                continue
            item = dict(url=url, platform="reddit", kind="comment", title=f"Commentaire sous « {title[:90]} »", author=e["who"], community="r/" + sub,
                        parent_url=group, parent_title=title, published_at=e["at"], quote=e["body"][:QUOTE_MAX], language="en")
            if e["who"] and OWN.search(e["who"]):
                auto.append(mine(item))
            elif SAID.search(e["body"]):
                cands.append(item)
    return cands, auto


def main():
    global KEY
    KEY = secret("youtube.key")
    os.makedirs(WORK, exist_ok=True)
    started = iso(time.time())
    path = os.path.join(WORK, "state.json")
    state = json.load(open(path)) if os.path.exists(path) else {}
    s = get(f"{API}/known", {"Authorization": "Bearer " + secret("veille.token")})
    if not isinstance(s, str):
        sys.exit(f"the veille answered {s}")
    by_url = {x["url"]: x for x in json.loads(s)["items"]}
    cands, auto = [], []
    for step in (youtube, reddit, transcripts):
        try:
            c, a = step(state, by_url) if step is not transcripts else (transcripts(state, by_url) if "--no-subs" not in sys.argv else ([], []))
            cands, auto = cands + c, auto + a
        except Exception as e:                   # (one source down: the others still count)
            notes.append(f"{step.__name__} : {type(e).__name__}")
    json.dump(state, open(path, "w"), ensure_ascii=False)
    seen, kept = set(), []
    for c in cands:
        if c["url"] not in seen or c.get("heard"):
            seen.add(c["url"])
            kept.append({**c, "i": len(kept)})
    json.dump(dict(started=started, notes=notes, auto=auto, candidates=kept), open(os.path.join(WORK, "candidates.json"), "w"), ensure_ascii=False, indent=1)
    open(os.path.join(WORK, "known.txt"), "w").write("\n".join(by_url))
    print(f"{len(kept)} candidates, {len(auto)} ready, known {len(by_url)}; notes: {'; '.join(notes) or 'none'}")


if __name__ == "__main__":
    main()
