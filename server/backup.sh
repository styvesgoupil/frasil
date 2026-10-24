#!/bin/bash
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# The River's database, each night on nimbus, as budge's (/home/deploy/scripts/budge-backup.sh): SQLite's online
# backup (sqlite3's .backup: safe while the service writes, its WAL included), taken inside the container by its own
# user (no sudo, nothing opens the volume's file from the host), gzipped; 30 kept in ~/apps/backups/frasil (700).
# Off-host (it holds emails and password hashes): only to an encrypted rclone remote, FRASIL_REMOTE (default
# gdrive-crypt:frasil, a "crypt" remote over gdrive: README.md), 30 days; without it, logged and skipped.
# deploy.sh puts this in ~/apps/frasil; add it to the crontab once (crontab -e):
#   15 4 * * * /home/deploy/apps/frasil/backup.sh
# Restore: docker compose stop; gunzip a day's copy over the volume's river.sqlite
# (/var/lib/docker/volumes/frasil_frasil-data/_data/, removing its -wal and -shm), sudo chown 10001 it; docker
# compose start.
set -u

BACKUP_DIR="/home/deploy/apps/backups/frasil"
LOG="$BACKUP_DIR/backup.log"
DATE="$(date +%Y-%m-%d)"
DST="$BACKUP_DIR/$DATE.db"
TS="$(date -Iseconds)"
REMOTE="${FRASIL_REMOTE:-gdrive-crypt:frasil}"
TAKE="import os, sqlite3; p = '/tmp/nightly.sqlite'; os.path.exists(p) and os.remove(p); d = sqlite3.connect(p)
sqlite3.connect('/data/river.sqlite').backup(d); d.close()"

mkdir -p "$BACKUP_DIR" && chmod 700 "$BACKUP_DIR"
if docker exec frasil-api python -c "$TAKE" 2>/dev/null \
   && docker cp -q frasil-api:/tmp/nightly.sqlite "$DST" && gzip -f "$DST"; then
    echo "$TS  frasil  OK    size=$(stat -c%s "$DST.gz" 2>/dev/null || echo "?")  file=$DATE.db.gz" >> "$LOG"
else
    rm -f "$DST" "$DST.gz"
    echo "$TS  frasil  FAIL  backup_or_gzip_failed (is frasil-api running?)" >> "$LOG"
    exit 1
fi
docker exec frasil-api rm -f /tmp/nightly.sqlite
ls -1t "$BACKUP_DIR"/*.db.gz 2>/dev/null | tail -n +31 | xargs -r rm -f

if ! rclone listremotes 2>/dev/null | grep -qx "${REMOTE%%:*}:"; then
    echo "$(date -Iseconds)  frasil  WARN  no_remote=${REMOTE%%:*} (off-host copy skipped)" >> "$LOG"
elif rclone copy --quiet "$DST.gz" "$REMOTE/" 2>/tmp/frasil-rclone.err; then
    echo "$(date -Iseconds)  frasil  OK    upload=$DATE.db.gz" >> "$LOG"
    rclone delete --min-age 30d --quiet "$REMOTE" 2>/dev/null || true
else
    echo "$(date -Iseconds)  frasil  WARN  upload_failed: $(head -c 200 /tmp/frasil-rclone.err | tr '\n' ' ')" \
        >> "$LOG"
fi
rm -f /tmp/frasil-rclone.err
exit 0
