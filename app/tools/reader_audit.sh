#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# What FRASIL says to a screen reader, checked by fm1-reader's audit (its rules: skill/fm1-screen-reader/RULES.md):
# the simulator's speakdump (every page, value, list row and help card: app/speak.c) and every message the app's tests
# make it say (FM1B_SAID_LOG), put into words with this build's words file as the reader's page does, then audited.
#   app/tools/reader_audit.sh [--spoken]     after ./build.sh --host; exit 1 on an error
# FM1_READER: an fm1-reader checkout (github.com/charlesvestal/fm1-reader), default ../fm1-reader beside the repository.
# Only its tools/audit.mjs is run (it reads the files and prints). --spoken: every message as it will be heard.
set -eu
cd "$(dirname "$0")/../.."
READER="${FM1_READER:-../fm1-reader}"
HOST=app/build/host/frasil_host
OUT=app/build/reader
[ -x "$HOST" ] || { echo "reader_audit: no $HOST (./build.sh --host)"; exit 2; }
[ -f "$READER/tools/audit.mjs" ] || { echo "reader_audit: no fm1-reader at $READER (FM1_READER)"; exit 2; }
WORDS="app/web/words/$(sed -n 's/.*words\/\([0-9a-f]*\.json\).*/\1/p' app/build/gen/ann_words.h)"
[ -f "$WORDS" ] || { echo "reader_audit: no $WORDS (./build.sh --host makes it)"; exit 2; }
rm -rf "$OUT"
mkdir -p "$OUT/dump"
printf 'wait 5000\nspeakdump\n' > "$OUT/dump.txt"
"$HOST" "$OUT/dump.txt" "$OUT/dump" | grep "$(printf '\t')" > "$OUT/all.tsv" || true
for t in app/tests/*.txt; do                  # (the tests' own results are theirs: only what they make it say)
    n="$(basename "$t" .txt)"
    mkdir -p "$OUT/$n"
    FM1B_SAID_LOG="$PWD/$OUT/said.tsv" "$HOST" "$t" "$OUT/$n" > "$OUT/$n.log" 2>&1 || true
done
[ -f "$OUT/said.tsv" ] && cat "$OUT/said.tsv" >> "$OUT/all.tsv"
sort -u "$OUT/all.tsv" -o "$OUT/all.tsv"
echo "reader_audit: $(wc -l < "$OUT/all.tsv" | tr -d ' ') messages, words $WORDS"
if [ "${1:-}" = "--spoken" ]; then
    node "$READER/tools/audit.mjs" "$OUT/all.tsv" --words "$WORDS" --spoken
else
    node "$READER/tools/audit.mjs" "$OUT/all.tsv" --words "$WORDS" --allow app/tools/reader_allow.txt
fi
