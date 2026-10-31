# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""The veille: an address's one form; the agent's token and what it opens; a mention added to and never replaced, new
activity; the page's list (its filters, its order, its pages), its changes, from the admin page only."""
import sqlite3

import httpx
import pytest

import store
from conftest import ORIGIN, db
from veille import canon

KEY, TOKEN, ADMIN_URL = "the-admin-key", "the-veille-token", "https://admin.test"
AUTH = {"Authorization": f"Bearer {TOKEN}"}
PAGE = {"X-Frasil-Admin": KEY, "Origin": ADMIN_URL}


@pytest.fixture
def c(make):
    return make(admin_key=KEY, veille_token=TOKEN, admin_url=ADMIN_URL)


def item(url="https://example.com/a", **kw):
    return {"url": url, "platform": "blog", "kind": "article", "title": "A title", "summary": "What they say.",
            "language": "en", "sentiment": "neutral", "priority": 2, **kw}


def ingest(c, *items):
    r = c.post("/api/veille/ingest", json={"run": {"started_at": "2026-10-31T06:00:00Z"}, "items": list(items)},
               headers={**AUTH, "Origin": ""})
    assert r.status_code == 200, r.text
    return r.json()


def listing(c, **q):
    r = c.get("/api/veille/mentions", params=q, headers=PAGE)
    assert r.status_code == 200, r.text
    return r.json()


def only(c):
    return listing(c, status="all")["items"][0]


def patch(c, mid, **body):
    r = c.patch(f"/api/veille/mentions/{mid}", json=body, headers=PAGE)
    assert r.status_code == 200, r.text
    return r.json()


@pytest.mark.parametrize("given, kept", [
    ("http://WWW.Example.com/Post/?utm_source=x&page=2&si=abc#top", "https://example.com/Post?page=2"),
    ("example.com/post/", "https://example.com/post"),
    ("https://forum.test/viewtopic.php?t=123&ref=x", "https://forum.test/viewtopic.php?t=123"),
    ("https://example.com:8443/a", "https://example.com:8443/a"),
    ("https://example.com:443/a", "https://example.com/a"),
    ("https://youtu.be/dQw4w9WgXcQ?si=xyz", "https://www.youtube.com/watch?v=dQw4w9WgXcQ"),
    ("https://youtube.com/shorts/dQw4w9WgXcQ?feature=share", "https://www.youtube.com/watch?v=dQw4w9WgXcQ"),
    ("https://m.youtube.com/watch?v=abc_-1&t=30s&list=PL1", "https://www.youtube.com/watch?v=abc_-1"),
    ("https://www.youtube.com/watch?v=abc&lc=Ugx9", "https://www.youtube.com/watch?v=abc&lc=Ugx9"),
    ("https://www.youtube.com/@frasil/?si=1", "https://www.youtube.com/@frasil"),
    ("https://old.reddit.com/r/Synthesizers/comments/ABC123/fm1_firmwares/?utm_source=share",
     "https://www.reddit.com/r/synthesizers/comments/abc123/"),
    ("https://np.reddit.com/r/synthesizers/comments/abc123", "https://www.reddit.com/r/synthesizers/comments/abc123/"),
    ("https://www.reddit.com/r/synthesizers/comments/abc123/slug/def456/",
     "https://www.reddit.com/r/synthesizers/comments/abc123/comment/def456/"),
    ("https://www.reddit.com/r/synthesizers/comments/abc123/slug/comment/def456/?context=3",
     "https://www.reddit.com/r/synthesizers/comments/abc123/comment/def456/"),
    ("https://www.reddit.com/r/synthesizers/comments/abc123/comment/def456/",
     "https://www.reddit.com/r/synthesizers/comments/abc123/comment/def456/"),
    ("https://redd.it/abc123", "https://www.reddit.com/comments/abc123/"),
    ("https://reddit.com/r/synthesizers/", "https://www.reddit.com/r/synthesizers"),
    ("https://github.com/StyvesGoupil/Frasil/issues/12#issuecomment-1", "https://github.com/styvesgoupil/frasil/issues/12"),
    ("https://github.com/styvesgoupil/frasil/discussions/3/", "https://github.com/styvesgoupil/frasil/discussions/3"),
    ("https://github.com/styvesgoupil/frasil.git", "https://github.com/styvesgoupil/frasil"),
    ("https://github.com/styvesgoupil/frasil/blob/main/README.md", "https://github.com/styvesgoupil/frasil/blob/main/README.md"),
    ("https://github.com/StyvesGoupil/Frasil/releases/tag/2.5/", "https://github.com/styvesgoupil/frasil/releases/tag/2.5"),
    ("https://twitter.com/someone/status/1?s=20&t=abc", "https://x.com/someone/status/1"),
    ("https://x.com/SomeOne/status/1/photo/1", "https://x.com/someone/status/1"),
    ("https://www.instagram.com/p/xyz/?igsh=abc", "https://instagram.com/p/xyz"),
])
def test_an_address_has_one_form(given, kept):
    assert canon(given) == kept
    assert canon(kept) == kept                 # (once is enough)


@pytest.mark.parametrize("url", ["", "javascript:alert(1)", "data:text/html,x", "ftp://example.com/a", "https://",
                                 "https://exa mple.com/a", "http://[bad", "https://example.com/" + "a" * 2048])
def test_what_is_no_web_address_is_refused(url):
    assert canon(url) is None


def test_the_agents_routes_take_only_the_token(c):
    for call in (lambda **k: c.get("/api/veille/known", **k), lambda **k: c.post("/api/veille/ingest", json={}, **k),
                 lambda **k: c.post("/api/veille/runs/x/finish", json={"status": "ok"}, **k)):
        assert call().status_code == 401
        assert call(headers={"Authorization": "Bearer nope"}).status_code == 401
        assert call(headers={"X-Frasil-Admin": KEY}).status_code == 401     # (the admin's key isn't theirs)
    assert c.get("/api/veille/known", headers=AUTH).json() == {"items": []}


def test_the_token_opens_nothing_else(c):
    assert c.get("/api/veille/mentions", headers=AUTH).status_code == 404
    assert c.get("/api/admin/overview", headers=AUTH).status_code == 404
    assert c.patch("/api/veille/mentions/1", json={"status": "seen"}, headers={**AUTH, "Origin": ADMIN_URL}).status_code == 404
    assert c.get("/api/me", headers=AUTH).status_code == 401                 # (it signs no one in)


def test_no_token_set_no_routes(make):
    c = make(admin_key=KEY, veille_token="")
    assert c.get("/api/veille/known", headers={"Authorization": "Bearer "}).status_code == 404
    assert c.post("/api/veille/ingest", json={}, headers=AUTH).status_code == 404


def test_a_new_mention(c):
    assert ingest(c, item("https://www.youtu.be/abc?si=1", platform="youtube", kind="video", author="Ana", is_own=True,
                          community="A channel", engagement=4, published_at="2026-10-29T14:02:00Z",
                          parent_url="http://example.com/parent/")) == {
        "run_id": listing(c)["last_run"]["id"], "inserted": 1, "updated": 0}
    m = only(c)
    assert m["url"] == canon("https://youtu.be/abc") == "https://www.youtube.com/watch?v=abc"
    assert m["status"] == "maybe_replied" and m["is_own"] is True and m["new_activity"] is False     # (a reply of one's own)
    assert m["parent_url"] == "https://example.com/parent" and m["published_at"] == "2026-10-29T14:02:00Z"
    assert m["first_found_at"] and m["first_found_at"] == m["last_seen_at"] == m["status_changed_at"]
    k = c.get("/api/veille/known", headers=AUTH).json()["items"]
    assert len(k) == 1 and {x: k[0][x] for x in ("url", "status", "engagement", "last_seen_at")} == {
        "url": m["url"], "status": "maybe_replied", "engagement": 4, "last_seen_at": m["last_seen_at"]}
    assert k[0]["title"] == m["title"] and k[0]["parent_url"] == m["parent_url"] and k[0]["priority"] == m["priority"]


def test_seen_again_what_was_done_stays(c):
    ingest(c, item(engagement=3, title="Old", author="Ana", community="Here"))
    mid = only(c)["id"]
    patch(c, mid, status="follow_up", notes="Call back")
    with db(c) as d:
        d.execute("UPDATE mentions SET first_found_at = 1000, status_changed_at = 2000, last_seen_at = 3000")
    run = ingest(c, item(engagement=9, title="New", summary="Newer.", sentiment="bug", priority=3, author="Bob"))
    assert (run["inserted"], run["updated"]) == (0, 1)
    m = only(c)
    assert m["title"] == "New" and m["summary"] == "Newer." and m["sentiment"] == "bug" and m["priority"] == 3
    assert m["engagement"] == 9 and m["last_seen_at"] > "1970-01-01T01" and m["last_run_id"] == run["run_id"]
    assert m["status"] == "follow_up" and m["notes"] == "Call back" and m["first_found_at"] == "1970-01-01T00:16:40Z"
    assert m["status_changed_at"] == "1970-01-01T00:33:20Z" and m["author"] == "Ana"     # (what is known stays)


def test_what_was_unknown_is_filled_in(c):
    ingest(c, item())
    ingest(c, item(author="Ana", community="Here", published_at="2026-10-29T14:02:00Z", parent_url="https://a.test/p"))
    m = only(c)
    assert (m["author"], m["community"], m["parent_url"]) == ("Ana", "Here", "https://a.test/p")
    assert m["published_at"] == "2026-10-29T14:02:00Z"


@pytest.mark.parametrize("when", ["0001-01-01T00:00:00+14:00", "9999-12-31T23:59:59Z", "1960-01-01T00:00:00Z"])
def test_a_date_out_of_range_is_refused_and_leaves_the_list_readable(c, when):
    r = c.post("/api/veille/ingest", json={"items": [item(published_at=when)]}, headers=AUTH)
    assert r.status_code == 422 and "items.0.published_at" in r.json()["detail"]
    r = c.post("/api/veille/ingest", json={"run": {"started_at": when}}, headers=AUTH)
    assert r.status_code == 422
    assert listing(c)["last_run"] is None


def test_a_body_without_a_length_is_refused(c):
    def chunks():
        yield b'{"items": []}'
    r = c.post("/api/veille/ingest", content=chunks(), headers={**AUTH, "Content-Type": "application/json",
                                                               "Transfer-Encoding": "chunked"})
    assert r.status_code == 411 and listing(c)["last_run"] is None


def test_the_same_address_in_two_forms_is_one_mention(c):
    run = ingest(c, item("https://old.reddit.com/r/s/comments/abc/one/"), item("https://www.reddit.com/r/S/comments/ABC/two"))
    assert (run["inserted"], run["updated"]) == (1, 1) and listing(c, status="all")["counts"]["total"] == 1


@pytest.mark.parametrize("status, before, after, lit", [
    ("replied", 5, 6, True), ("ignored", 5, 6, True), ("seen", 5, 6, True),
    ("replied", 5, 5, False), ("replied", 5, 4, False), ("replied", None, 6, False), ("replied", 5, None, False),
    ("new", 5, 6, False), ("follow_up", 5, 6, False)])
def test_new_activity(c, status, before, after, lit):
    ingest(c, item(engagement=before))
    patch(c, only(c)["id"], status=status)
    ingest(c, item(engagement=after))
    m = only(c)
    assert m["new_activity"] is lit and m["status"] == status
    assert m["engagement"] == (before if after is None else after)     # (none seen: the last one known stays)


def test_a_status_set_clears_new_activity(c):
    ingest(c, item(engagement=1))
    mid = only(c)["id"]
    patch(c, mid, status="replied")
    ingest(c, item(engagement=2))
    assert listing(c)["counts"]["new_activity"] == 1
    assert patch(c, mid, notes="Later.")["item"]["new_activity"] is True    # (a note doesn't)
    out = patch(c, mid, status="replied")                                   # (the same status again does)
    assert out["item"]["new_activity"] is False and out["counts"]["new_activity"] == 0
    assert patch(c, mid, status="seen", new_activity=True)["item"]["new_activity"] is True   # (UNDO's)


def test_a_batch_is_all_or_nothing(c):
    bad = [{**item(), "platform": "myspace"}, {**item(), "priority": 4}, {**item(), "sentiment": "angry"},
           {**item(), "url": "javascript:alert(1)"}, {**item(), "parent_url": "ftp://x.test/a"}, {**item(), "title": " "},
           {**item(), "title": "x" * 501}, {**item(), "published_at": "yesterday"}, {**item(), "engagement": -1}]
    for b in bad:
        r = c.post("/api/veille/ingest", json={"items": [item("https://example.com/ok"), b]}, headers=AUTH)
        assert r.status_code == 422 and "items.1" in r.json()["detail"], (b, r.text)
    assert listing(c, status="all")["counts"]["total"] == 0 and listing(c)["last_run"] is None
    r = c.post("/api/veille/ingest", json={"items": [item(f"https://example.com/{n}") for n in range(1001)]}, headers=AUTH)
    assert r.status_code == 422


def test_an_empty_pass_counts_as_one(c):
    run = ingest(c)
    assert run["inserted"] == run["updated"] == 0
    r = c.post(f"/api/veille/runs/{run['run_id']}/finish", json={"status": "partial", "notes": "ModWiggler down"},
               headers={**AUTH, "Origin": ""})
    assert r.status_code == 200 and r.json() == {"ok": True}
    last = listing(c)["last_run"]
    assert last["status"] == "partial" and last["notes"] == "ModWiggler down" and last["finished_at"]
    assert c.post("/api/veille/runs/nope/finish", json={"status": "ok"}, headers=AUTH).status_code == 404
    assert c.post(f"/api/veille/runs/{run['run_id']}/finish", json={"status": "fine"}, headers=AUTH).status_code == 422


def test_a_big_batch_passes_where_a_river_change_would_not(c):
    r = c.post("/api/veille/ingest", headers=AUTH, json={
        "items": [item(f"https://example.com/{n}", summary="word " * 300) for n in range(400)]})
    assert r.status_code == 200 and r.json()["inserted"] == 400
    r = c.post("/api/veille/ingest", headers={**AUTH, "Content-Type": "text/plain"}, content="{}")
    assert r.status_code == 415


def test_the_pages_routes_need_the_key(c):
    ingest(c, item())
    mid = only(c)["id"]
    assert c.get("/api/veille/mentions").status_code == 404
    assert c.patch(f"/api/veille/mentions/{mid}", json={"status": "seen"}, headers={"Origin": ADMIN_URL}).status_code == 404
    assert c.patch("/api/veille/mentions", json={"ids": [mid], "status": "seen"}, headers={"Origin": ADMIN_URL}).status_code == 404


def test_the_pages_changes_come_from_the_admin_page(c):
    ingest(c, item())
    mid = only(c)["id"]
    for origin in (ORIGIN, "https://evil.test", ""):
        r = c.patch(f"/api/veille/mentions/{mid}", json={"status": "seen"}, headers={"X-Frasil-Admin": KEY, "Origin": origin})
        assert r.status_code == 403, origin
    assert only(c)["status"] == "new"


def test_the_list(c):
    ingest(c, item("https://a.test/1", platform="reddit", kind="thread", title="Réponse à Ana", author="ana_w",
                   community="r/synthesizers", sentiment="question", priority=3),
           item("https://a.test/2", platform="youtube", kind="video", title="Un drone", summary="100% ambient",
                sentiment="positive", priority=1, published_at="2026-10-01T00:00:00Z"),
           item("https://a.test/3", title="Le dernier", sentiment="bug", priority=2))
    titles = lambda **q: [m["title"] for m in listing(c, **q)["items"]]
    assert titles() == ["Réponse à Ana", "Le dernier", "Un drone"]            # (the priority first, in one day)
    assert titles(platform="youtube") == ["Un drone"]
    assert titles(sentiment="bug") == ["Le dernier"]
    assert titles(priority=3) == ["Réponse à Ana"]
    assert titles(q="REPONSE") == ["Réponse à Ana"]                           # (case and accents aside)
    assert titles(q="ana_w") == titles(q="synthesizers ana") == ["Réponse à Ana"]
    assert titles(q="ana x") == []                                            # (every word)
    assert titles(q="100%") == ["Un drone"] and titles(q="%") == ["Un drone"]  # (a % is a %)
    got = listing(c)
    assert got["platforms"] == {"blog": 1, "reddit": 1, "youtube": 1}
    assert (got["counts"]["new"], got["counts"]["total"], got["counts"]["inbox"]) == (3, 3, 3)
    assert c.get("/api/veille/mentions?status=nope", headers=PAGE).status_code == 422
    assert c.get("/api/veille/mentions?cursor=1;drop", headers=PAGE).status_code == 422


def test_its_views(c):
    ingest(c, *[item(f"https://a.test/{n}", engagement=1) for n in range(5)])
    a, b, d, e, f = [m["id"] for m in listing(c)["items"]]
    for mid, s in ((a, "seen"), (b, "follow_up"), (d, "replied"), (e, "ignored")):
        patch(c, mid, status=s)
    ingest(c, item("https://a.test/2", engagement=2), *[item(f"https://a.test/{n}", engagement=2) for n in (0, 1, 3, 4)])
    sets = {v: {m["id"] for m in listing(c, status=v)["items"]} for v in
            ("inbox", "activity", "new", "seen", "follow_up", "replied", "ignored", "all")}
    assert sets["new"] == {f} and sets["follow_up"] == {b} and sets["seen"] == {a} and sets["all"] == {a, b, d, e, f}
    assert sets["activity"] == {a, d, e} and sets["inbox"] == {a, d, e, f}
    n = listing(c)["counts"]
    assert (n["new"], n["seen"], n["follow_up"], n["replied"], n["ignored"]) == (1, 1, 1, 1, 1)
    assert (n["new_activity"], n["inbox"], n["total"]) == (3, 4, 5)


def test_its_pages_lose_nothing_as_mentions_leave_it(c):
    ingest(c, *[item(f"https://a.test/{n}") for n in range(7)])
    seen, cursor = [], ""
    while True:
        got = listing(c, limit=3, cursor=cursor)
        seen += [m["id"] for m in got["items"]]
        patch(c, got["items"][0]["id"], status="ignored")     # (dealt with while the next page is still to come)
        cursor = got["next_cursor"]
        if not cursor:
            break
    assert len(seen) == len(set(seen)) == 7


def test_its_days_are_the_pages(c):
    ingest(c, item())
    with db(c) as d:
        d.execute("UPDATE mentions SET first_found_at = 1792980000")     # (2026-10-26 02:00 UTC)
    assert listing(c, status="all")["items"][0]["day"] == "2026-10-26"
    assert listing(c, status="all", tz=240)["items"][0]["day"] == "2026-10-25"     # (22:00 the evening before, Toronto)


def test_changes_at_once(c):
    ingest(c, *[item(f"https://a.test/{n}") for n in range(3)])
    ids = [m["id"] for m in listing(c)["items"]]
    r = c.patch("/api/veille/mentions", json={"ids": ids[:2] + [999], "status": "ignored"}, headers=PAGE)
    assert r.status_code == 200 and r.json()["updated"] == 2 and r.json()["counts"]["ignored"] == 2
    assert c.patch("/api/veille/mentions", json={"ids": [], "status": "seen"}, headers=PAGE).status_code == 422
    assert c.patch("/api/veille/mentions", json={"ids": ids, "status": "gone"}, headers=PAGE).status_code == 422
    assert c.patch("/api/veille/mentions/999", json={"status": "seen"}, headers=PAGE).status_code == 404
    assert c.patch(f"/api/veille/mentions/{ids[0]}", json={"notes": "x" * 5001}, headers=PAGE).status_code == 422


class Discord:
    """stands in for Discord: what the bot sent, what it answers"""

    def __init__(self, status=200):
        self.status, self.sent = status, []

    def request(self, method, url, headers=None, json=None):
        self.sent.append((method, url, headers, json))
        if self.status is None:
            raise httpx.ConnectError("down")
        return httpx.Response(self.status, json={"id": "777"})


@pytest.fixture
def bot(c):
    c.app.state.settings.discord_token = "the-bot-token"
    c.app.state.http = Discord()
    return c.app.state.http


def post(c, mid, **h):
    return c.post(f"/api/veille/mentions/{mid}/discord", json={}, headers={**PAGE, **h})


def test_a_mention_posted_to_made_with_frasil(c, bot):
    ingest(c, item("https://youtu.be/abc", platform="youtube", kind="video", title="A *drone* [live]", author="Ana_B",
                   summary="Ce que Styves doit savoir."))
    assert listing(c)["discord"] is True
    r = post(c, only(c)["id"])
    assert r.status_code == 200
    u = r.json()["item"]["discord_url"]
    assert u == "https://discord.com/channels/1563986548209356830/1563988202677739541/777" == only(c)["discord_url"]
    method, url, headers, body = bot.sent[0]
    assert method == "POST" and url.endswith("/channels/1563988202677739541/messages")
    assert headers["Authorization"] == "Bot the-bot-token"
    assert body["allowed_mentions"] == {"parse": []}                    # (no @everyone from a title)
    assert body["content"] == r"**A \*drone\* \[live\]** · Ana\_B" + "\nhttps://www.youtube.com/watch?v=abc"
    assert "Styves" not in body["content"]                              # (the summary is ours)
    assert only(c)["status"] == "new"


def test_a_comment_or_a_listing_goes_without_our_french_title(c, bot):
    ingest(c, item("https://a.test/1", kind="comment", title="Commentaire sous « X »", author="Ana"),
           item("https://a.test/2", kind="other", title="Liste (fiche FRASIL)"))
    a, b = sorted(m["id"] for m in listing(c, status="all")["items"])
    post(c, a), post(c, b)
    assert [x[3]["content"] for x in bot.sent] == ["Ana\nhttps://a.test/1", "https://a.test/2"]


def test_once_only(c, bot):
    ingest(c, item())
    mid = only(c)["id"]
    assert post(c, mid).status_code == 200
    assert post(c, mid).status_code == 409 and len(bot.sent) == 1


def test_a_post_refused_or_lost_can_be_tried_again(c, bot):
    ingest(c, item())
    mid = only(c)["id"]
    for bad in (403, None):
        bot.status = bad
        assert post(c, mid).status_code == 502 and only(c)["discord_url"] is None
    bot.status = 200
    assert post(c, mid).status_code == 200


def test_a_post_with_no_clear_answer_keeps_its_nonce(c, bot):
    ingest(c, item())
    mid = only(c)["id"]
    bot.status = None                                                   # (it may have gone)
    post(c, mid)
    bot.status = 403                                                    # (refused: nothing went)
    post(c, mid)
    bot.status = 200
    post(c, mid)
    nonces = [x[3]["nonce"] for x in bot.sent]
    assert nonces[0] == nonces[1] != nonces[2] and bot.sent[0][3]["enforce_nonce"] is True


def test_a_title_cant_put_a_live_link_in_the_post(c, bot):
    ingest(c, item(title="Free https://evil.test/x", author="http://evil.test"))
    post(c, only(c)["id"])
    text = bot.sent[0][3]["content"]
    assert "https://evil" not in text and "http://evil" not in text and "https\\://evil.test" in text


def test_no_moments_is_none(c):
    ingest(c, item(moments=[]))
    assert only(c)["moments"] == []


def test_a_moment_only_links_a_second_on_a_video(c):
    ingest(c, item("https://a.test/1", moments=[{"at": 5}]))
    assert only(c)["moments"][0]["url"] == "https://a.test/1"


def test_a_cut_off_post_is_freed_at_the_start(c, bot):
    ingest(c, item())
    with db(c) as d:
        d.execute("UPDATE mentions SET discord_url = ''")
    store.init(c.app.state.settings.db)
    assert post(c, only(c)["id"]).status_code == 200


def test_a_keeper_stays_and_doesnt_call_for_attention(c):
    ingest(c, item(engagement=1))
    mid = only(c)["id"]
    assert patch(c, mid, status="keeper")["counts"]["keeper"] == 1
    ingest(c, item(engagement=9))
    m = listing(c, status="keeper")["items"][0]
    assert m["status"] == "keeper" and m["new_activity"] is False and listing(c)["items"] == []


def test_taken_back(c, bot):
    ingest(c, item())
    mid = only(c)["id"]
    assert c.delete(f"/api/veille/mentions/{mid}/discord", headers=PAGE).status_code == 404     # (not there)
    post(c, mid)
    r = c.delete(f"/api/veille/mentions/{mid}/discord", headers=PAGE)
    assert r.status_code == 200 and r.json()["item"]["discord_url"] is None
    assert bot.sent[-1][:2] == ("DELETE", "https://discord.com/api/v10/channels/1563988202677739541/messages/777")
    assert post(c, mid).status_code == 200                              # (and can go again)
    bot.status = 404                                                    # (deleted by hand on Discord: the same)
    assert c.delete(f"/api/veille/mentions/{mid}/discord", headers=PAGE).status_code == 200


def test_only_the_page_posts(c, bot):
    ingest(c, item())
    mid = only(c)["id"]
    assert c.post(f"/api/veille/mentions/{mid}/discord", json={}, headers={**AUTH, "Origin": ADMIN_URL}).status_code == 404
    assert c.post(f"/api/veille/mentions/{mid}/discord", json={}, headers={"Origin": ADMIN_URL}).status_code == 404
    assert post(c, mid, Origin="https://evil.test").status_code == 403
    assert post(c, 999).status_code == 404 and not bot.sent


def test_no_bot_no_action(c):
    ingest(c, item())
    assert listing(c)["discord"] is False and post(c, only(c)["id"]).status_code == 404


def test_a_database_from_before_them_gets_the_columns(tmp_path):
    p = str(tmp_path / "old.sqlite")
    store.init(p)
    old = sqlite3.connect(p)
    old.execute("ALTER TABLE mentions DROP COLUMN discord_url")
    old.execute("ALTER TABLE mentions DROP COLUMN moments")
    old.commit(), old.close()
    store.init(p)
    assert {"discord_url", "moments"} <= {c[1] for c in sqlite3.connect(p).execute("PRAGMA table_info(mentions)")}


def test_where_a_video_says_it(c, bot):
    v = dict(platform="youtube", kind="video")
    ingest(c, item("https://youtu.be/abc", **v, moments=[{"at": 754, "quote": "and then FRASIL"}, {"at": 12, "quote": ""}]))
    m = only(c)
    assert [(x["at"], x["quote"]) for x in m["moments"]] == [(12, ""), (754, "and then FRASIL")]  # (in order)
    assert m["moments"][1]["url"] == "https://www.youtube.com/watch?v=abc&t=754s"
    ingest(c, item("https://youtu.be/abc", **v))
    assert only(c)["moments"] == m["moments"]                           # (none sent: as they were)
    ingest(c, item("https://youtu.be/abc", **v, moments=[{"at": 30, "quote": "again"}]))
    assert [x["at"] for x in only(c)["moments"]] == [30]
    post(c, m["id"])
    assert bot.sent[0][3]["content"].endswith("https://www.youtube.com/watch?v=abc&t=30s")     # (posted where it's said)
    r = c.post("/api/veille/ingest", json={"items": [item(moments=[{"at": -1}])]}, headers=AUTH)
    assert r.status_code == 422
    r = c.post("/api/veille/ingest", json={"items": [item(moments=[{"at": 1, "quote": "x" * 301}])]}, headers=AUTH)
    assert r.status_code == 422 and "items.0.moments" in r.json()["detail"]
    assert item()["url"] and listing(c, status="all")["counts"]["total"] == 1


def own(url, parent, **kw):
    return item(url, kind="comment", parent_url=parent, is_own=True, published_at="2026-10-29T12:00:00Z", **kw)


def kid(url, parent, at="2026-10-28T12:00:00Z", **kw):
    return item(url, kind="comment", parent_url=parent, published_at=at, **kw)


V = "https://www.youtube.com/watch?v=abc"


def test_a_video_and_its_comments_are_one(c):
    ingest(c, item("https://a.test/alone", priority=1), kid(V + "&lc=2", V, priority=3), kid(V + "&lc=1", V, at="2026-10-20T00:00:00Z"),
           item(V, platform="youtube", kind="video", published_at="2026-10-30T00:00:00Z"),
           kid("https://www.youtube.com/watch?v=def&lc=9", "https://www.youtube.com/watch?v=def", parent_title="Their video"))
    got = listing(c)
    assert [m["url"] for m in got["items"]] == [V, V + "&lc=1", V + "&lc=2", "https://www.youtube.com/watch?v=def&lc=9",
                                                "https://a.test/alone"]      # (whole, what it is under first; by priority)
    assert {m["group"] for m in got["items"][:3]} == {V} and got["items"][3]["parent_title"] == "Their video"
    assert got["counts"]["total"] == got["counts"]["new"] == got["counts"]["inbox"] == 3
    assert got["platforms"]["blog"] == 3


def test_any_of_a_group_that_fits_brings_it_all(c):
    ingest(c, item(V, platform="youtube", kind="video"), kid(V + "&lc=1", V, title="A comment", summary="needle here"))
    assert len(listing(c, q="needle")["items"]) == 2


def test_its_pages_count_groups_and_never_cut_one(c):
    ingest(c, *[x for n in range(5) for x in (item(f"https://a.test/{n}"), kid(f"https://a.test/{n}/c", f"https://a.test/{n}"))])
    seen, cursor, pages = [], "", 0
    while True:
        got = listing(c, limit=2, cursor=cursor)
        assert len(got["items"]) == 4 or not got["next_cursor"]
        seen += [m["id"] for m in got["items"]]
        pages, cursor = pages + 1, got["next_cursor"]
        if not cursor:
            break
    assert pages == 3 and len(seen) == len(set(seen)) == 10


def test_the_owners_reply_puts_what_came_before_at_maybe_replied(c):
    ingest(c, item(V, platform="youtube", kind="video", published_at="2026-10-20T00:00:00Z"), kid(V + "&lc=1", V),
           kid(V + "&lc=2", V, at="2026-10-30T00:00:00Z"), item("https://a.test/other"))
    ingest(c, own(V + "&lc=me", V))
    s = {m["url"]: m["status"] for m in listing(c, status="all")["items"]}
    assert s[V] == s[V + "&lc=1"] == s[V + "&lc=me"] == "maybe_replied"
    assert s[V + "&lc=2"] == "new" and s["https://a.test/other"] == "new"    # (said after: still to read; elsewhere: untouched)
    urls = lambda v: {m["url"] for m in listing(c, status=v)["items"]}
    assert V in urls("maybe_replied") and V in urls("inbox")                 # (the group is in both)
    assert listing(c)["counts"]["maybe_replied"] == 1


def test_the_owners_own_post_stays_to_be_seen(c):
    ingest(c, item("https://github.com/a/b/issues/1", is_own=True))
    assert only(c)["status"] == "new"


def test_what_was_dealt_with_stays(c):
    ingest(c, item(V, platform="youtube", kind="video", published_at="2026-10-20T00:00:00Z"))
    patch(c, only(c)["id"], status="ignored")
    ingest(c, own(V + "&lc=me", V))
    assert next(m for m in listing(c, status="all")["items"] if m["url"] == V)["status"] == "ignored"


def test_the_owners_reply_is_no_new_activity_someone_elses_is(c):
    ingest(c, item(V, platform="youtube", kind="video", engagement=5, published_at="2026-10-20T00:00:00Z"))
    patch(c, only(c)["id"], status="replied")
    ingest(c, item(V, platform="youtube", kind="video", engagement=6), own(V + "&lc=me", V))
    head = lambda: next(m for m in listing(c, status="all")["items"] if m["url"] == V)
    assert head()["new_activity"] is False and head()["engagement"] == 6
    ingest(c, item(V, platform="youtube", kind="video", engagement=7))
    assert head()["new_activity"] is True


def test_their_words_kept_and_searched(c):
    ingest(c, item(quote="  Really enjoying   it\n\n\n\nso far  "))
    assert only(c)["quote"] == "Really enjoying   it\n\nso far"
    ingest(c, item())
    assert only(c)["quote"].startswith("Really")                        # (none sent: as they were)
    assert len(listing(c, q="enjoying")["items"]) == 1


def test_the_tabs_count_under_the_filters(c):
    ingest(c, item("https://a.test/1", platform="youtube"), item("https://a.test/2", platform="youtube"), item("https://a.test/3"))
    a = listing(c, platform="youtube")["items"][0]["id"]
    patch(c, a, status="replied")
    got = listing(c, platform="youtube", status="all")
    assert (got["tabs"]["new"], got["tabs"]["replied"], got["tabs"]["total"]) == (1, 1, 2)
    assert (got["counts"]["new"], got["counts"]["total"]) == (2, 3)     # (the head: all of it)
