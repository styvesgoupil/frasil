#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""A local preview of the assembled site, as the host will serve it: build/website/ at the root, so the site is at
/frasil/ (the sub-path, for real); a page that isn't there gets frasil/404.html, with the status 404; .wasm is
application/wasm. Audio worklets run on localhost as on https.   serve.py ROOT PORT SITE"""
import http.server
import sys
from functools import partial
from pathlib import Path

ROOT, PORT, SITE = Path(sys.argv[1]).resolve(), int(sys.argv[2]), sys.argv[3]


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map,
                      ".wasm": "application/wasm", ".js": "text/javascript", ".svg": "image/svg+xml"}

    def send_error(self, code, message=None, explain=None):
        page = ROOT / SITE / "404.html"
        if code == 404 and self.path.startswith(f"/{SITE}/") and page.exists():
            body = page.read_bytes()
            self.send_response(404)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(body)
            return
        super().send_error(code, message, explain)

    def log_request(self, code="-", size="-"):        # only what went wrong
        if str(code)[:1] in ("4", "5"):
            super().log_request(code, size)


class Server(http.server.ThreadingHTTPServer):
    request_queue_size = 128                          # a browser opens many connections at once (the default is 5)
    daemon_threads = True


if __name__ == "__main__":
    print(f"serving  http://localhost:{PORT}/{SITE}/  (ctrl-C to stop)")
    Server(("127.0.0.1", PORT), partial(Handler, directory=str(ROOT))).serve_forever()
