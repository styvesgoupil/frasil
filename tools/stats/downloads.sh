#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# FRASIL's download count, from GitHub's own counters: the .fwsc assets of every release of styvesgoupil/frasil, summed
# into the JSON the main site reads (/stats/downloads.json). Runs on nimbus, hourly, as deploy (copied to
# ~/apps/frasil-stats/); the site's deploy leaves /var/www/frasilsynth.com/stats/ alone (rsync --exclude /stats/).
#   17 * * * * ~/apps/frasil-stats/downloads.sh >/dev/null 2>&1
#   tools/stats/downloads.sh [OUT]     OUT defaults to /var/www/frasilsynth.com/stats/downloads.json
# On any failure the existing file stays as it is, and the exit code is not 0 with one line on stderr.
set -eu
out=${1:-/var/www/frasilsynth.com/stats/downloads.json}
fail() { echo "downloads.sh: $*" >&2; exit 1; }
dir=$(dirname "$out")
mkdir -p "$dir" || fail "cannot create $dir"
tmp=$(mktemp "$dir/.downloads.XXXXXX") || fail "cannot write in $dir"
trap 'rm -f "$tmp" "$tmp.api" "$tmp.err"' EXIT
curl -fsS --max-time 30 -H "User-Agent: frasil-stats" -H "Accept: application/vnd.github+json" \
  -o "$tmp.api" "https://api.github.com/repos/styvesgoupil/frasil/releases?per_page=100" 2>"$tmp.err" \
  || fail "GitHub API: $(head -n 1 "$tmp.err")"
python3 - "$tmp.api" >"$tmp" <<'PY' || fail "unexpected answer from GitHub"
import json, sys, datetime
rels = json.load(open(sys.argv[1]))
if not isinstance(rels, list): sys.exit(1)
out = []
for r in rels:
    n = sum(a["download_count"] for a in r["assets"] if a["name"].endswith(".fwsc"))
    out.append({"tag": r["tag_name"], "downloads": n, "published": r.get("published_at")})
json.dump({"total": sum(r["downloads"] for r in out), "releases": out,
           "updated": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")}, sys.stdout)
print()
PY
chmod 644 "$tmp"
mv "$tmp" "$out"
