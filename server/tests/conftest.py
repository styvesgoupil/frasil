# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""The River's tests: each one a fresh service (its own database, its own limits) behind https://river.test, the
schema the simulator writes (server/schema.json: made here if it isn't there yet) and the River's example backup
(build/website/river/example.frasil: its scenes are real scenes of the schema's layout; made with the simulator if it
isn't there)."""
import json
import re
import struct
import subprocess
import sys
import zlib
from pathlib import Path

import pytest
from starlette.testclient import TestClient

SERVER = Path(__file__).resolve().parent.parent
REPO = SERVER.parent
sys.path.insert(0, str(SERVER))

import mail  # noqa: E402
import store  # noqa: E402
from app import create_app  # noqa: E402

ORIGIN = "https://river.test"
ADMIN = "admin@river.test"
PASSWORD = "drone in the fog"


def pytest_sessionstart(session):
    if not (SERVER / "schema.json").exists():
        subprocess.run([sys.executable, str(SERVER / "make_schema.py")], check=True)


@pytest.fixture(scope="session")
def example(tmp_path_factory):
    p = REPO / "build" / "website" / "river" / "example.frasil"
    if not p.exists():
        work = tmp_path_factory.mktemp("example")
        subprocess.run([str(REPO / "app" / "build" / "host" / "frasil_host"), str(REPO / "website" / "river" /
                                                                                  "example.txt"), str(work)],
                       check=True, capture_output=True)
        p = work / "example.frasil"
    return p.read_bytes()


@pytest.fixture(scope="session")
def scenes(example):
    """the example's saved scenes (ABYSS, CATHEDRAL, GLACIER, AURORA: its bass's sequence on), each its scene_t: its
    bytes in the scenes' record (object 2), then its bass's (object 3)"""
    pos, n, objs = 16, struct.unpack_from("<H", example, 6)[0], {}
    for _ in range(n):
        obj, ln, _crc = struct.unpack_from("<HHI", example, pos)
        pos += 8
        objs[obj] = example[pos:pos + ln]
        pos += ln
    sch = json.loads((SERVER / "schema.json").read_text())
    p, q, n, b = objs[sch["objs"]["scenes"]], objs[sch["objs"]["bseqs"]], sch["scene_t"]["rec"], sch["bseqs_t"]["scene"]
    ps, qs, used = sch["scenes_t"]["s"], sch["bseqs_t"]["s"], p[4] | p[5] << 8
    return [p[ps + i * n:ps + (i + 1) * n] + q[qs + i * b:qs + (i + 1) * b] for i in range(16) if used >> i & 1]


def image(objs, magic=b"FRBK", version=1):
    """a backup file (fm1_backup.py's build)"""
    head = magic + struct.pack("<HHI", version, len(objs), 0)
    out = bytearray(head + struct.pack("<I", zlib.crc32(head)))
    for obj, data in objs:
        out += struct.pack("<HHI", obj, len(data), zlib.crc32(data)) + data
    return bytes(out + struct.pack("<I", zlib.crc32(out)))


@pytest.fixture
def make(tmp_path):
    """make(**settings) -> a client of a fresh service (the same database for every client of this test)"""
    mail.SENT.clear()

    def make(app=None, **kw):
        if app is None:
            kw = {"db": str(tmp_path / "river.sqlite"), "url": ORIGIN, "schema": str(SERVER / "schema.json"),
                  "admins": [ADMIN], **kw}
            app = create_app(store.Settings(**kw))
        c = TestClient(app, base_url=ORIGIN, follow_redirects=False)
        c.headers["Origin"] = ORIGIN
        return c
    return make


@pytest.fixture
def client(make):
    return make()


def link(kind, to=None):
    """the token of the last email's link (#verify=, #reset=), to whom it went"""
    for m in reversed(mail.SENT):
        if to in (None, m["to"]):
            got = re.search(rf"#{kind}=([\w-]+)", m["body"])
            if got:
                return got.group(1)
    return None


def signup(c, email="ana@river.test", name="Ana", verified=False, password=PASSWORD):
    r = c.post("/api/auth/register", json={"email": email, "password": password, "display_name": name},
               headers={"CF-Connecting-IP": f"10.0.{len(email)}.{sum(email.encode()) % 256}"})   # (not one IP's 10)
    assert r.status_code == 201, r.text
    if verified:
        assert c.post("/api/auth/verify", json={"token": link("verify", email)}).status_code == 200
    return r.json()


def other(c, make, **kw):
    """another browser, on the same service, signed up"""
    d = make(app=c.app)
    signup(d, **kw)
    return d


def db(c):
    return store.connect(c.app.state.settings.db)
