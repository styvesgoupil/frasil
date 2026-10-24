# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""FRASIL River's service (river.frasilsynth.com/api/): accounts, each one's library of shards and backups, the
public River. FastAPI and SQLite; Caddy in front (TLS, the site's static files), the same origin for both.

    uvicorn app:create_app --factory --host 0.0.0.0 --port 8080       (the settings: store.Settings, .env.example)
"""
from urllib.parse import urlsplit

import httpx
from fastapi import FastAPI, Request
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse

import auth
import guard
import shards
import store
from scenes import Schema


def create_app(settings=None):
    s = settings or store.Settings.from_env()
    app = FastAPI(title="FRASIL River", docs_url=None, redoc_url=None, openapi_url=None)
    u = urlsplit(s.url)
    app.state.settings, app.state.origin = s, f"{u.scheme}://{u.netloc}"
    app.state.schema, app.state.limiter = Schema(s.schema), guard.Limiter()
    app.state.salt, app.state.kept = store.init(s.db, app.state.schema), 0.0
    app.state.http = httpx.Client(timeout=15)     # (Google's token endpoint)
    app.middleware("http")(guard.same_origin)

    @app.exception_handler(RequestValidationError)
    def invalid(_request: Request, e: RequestValidationError):
        first = e.errors()[0] if e.errors() else {}
        where = ".".join(str(x) for x in first.get("loc", ())[1:]) or "the request"
        return JSONResponse({"detail": f"{where}: {first.get('msg', 'not valid')}"}, 422)

    @app.get("/api/health")
    def health():
        db = store.connect(s.db)
        try:
            db.execute("SELECT 1").fetchone()
        finally:
            db.close()
        return {"ok": True, "layout": app.state.schema.layout, "firmware": app.state.schema.firmware}

    app.include_router(auth.r)
    app.include_router(shards.r)
    if s.static:                                  # (a local run: the River's page at /, as Caddy serves it)
        from fastapi.staticfiles import StaticFiles
        app.mount("/", StaticFiles(directory=s.static, html=True))
    return app
