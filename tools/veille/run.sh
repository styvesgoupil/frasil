#!/bin/bash
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# A pass of the veille (launchd on the M3, com.styves.frasil-veille.plist): the gathering, then Claude Code on Sonnet to judge what
# is new and search the rest (prompt.md: no shell, it reads, writes decisions.json and searches: what it reads comes from the web),
# then the sending. No decisions: what is ready is sent, the pass marked in error (the page shows it in red), whatever Claude's
# exit code.   tools/veille/install.sh puts it in place
export PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin"
export DISABLE_OMC=1
HERE="$HOME/.local/share/frasil-veille"
WORK="$HOME/.local/state/frasil-veille"
mkdir -p "$WORK"
cd "$WORK" || exit 1
rm -f decisions.json sent
echo "=== $(date '+%F %T')"
python3 "$HERE/collect.py" --no-subs || { echo "=== collect failed"; exit 1; }
perl -e 'alarm shift; exec @ARGV' 1800 claude -p "$(cat "$HERE/prompt.md")" --model sonnet --strict-mcp-config --no-session-persistence \
    --permission-mode dontAsk --allowedTools "Read(./**)" "Edit(./**)" "Read(~/.local/state/frasil-veille/**)" "Edit(~/.local/state/frasil-veille/**)" WebSearch WebFetch
code=$?
echo "=== claude exit $code"
if ! python3 -c 'import json; d = json.load(open("decisions.json")); assert isinstance(d["keep"], list)' 2>/dev/null; then
    python3 "$HERE/ingest.py" --failed "Claude Code n'a pas rendu ses décisions (code $code)"
elif ! python3 "$HERE/ingest.py"; then
    python3 "$HERE/ingest.py" --failed "Décisions refusées par la veille"
fi
tail -c 2000000 run.log > run.log.tmp 2>/dev/null && mv run.log.tmp run.log
