#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# FRASIL River's service, to nimbus (on Styves's go only): the schema made from the simulator (make_schema.py: run
# ./build.sh --host first), the tests, then server/ to ~/apps/frasil (rsync: never its .env nor its data) and
# docker compose up -d --build there; then its health. The page itself is the site's (tools/website.sh).
#   server/deploy.sh [--no-tests]          FRASIL_HOST (deploy@204.168.236.118), FRASIL_DIR (apps/frasil)
set -eu
cd "$(dirname "$0")"
HOST="${FRASIL_HOST:-deploy@204.168.236.118}"
DIR="${FRASIL_DIR:-apps/frasil}"
python3 make_schema.py
if [ "${1:-}" != "--no-tests" ]; then
    [ -x .venv/bin/python ] || { echo "deploy: no server/.venv to test with (README.md, \"Run it here\")"; exit 1; }
    .venv/bin/python -m pytest -q
fi
echo "[deploy] syncing to $HOST:~/$DIR"
ssh "$HOST" "mkdir -p $DIR"
rsync -az --delete --exclude .venv --exclude __pycache__ --exclude .pytest_cache --exclude tests --exclude .env \
    --exclude '*.sqlite' --exclude '*.sqlite-*' ./ "$HOST:$DIR/"
ssh "$HOST" "test -f $DIR/.env" || { echo "deploy: no ~/$DIR/.env on $HOST: copy .env.example there, fill it in, \
then run this again"; exit 1; }
echo "[deploy] docker compose up -d --build"
ssh "$HOST" "cd $DIR && docker compose up -d --build"
sleep 3
echo "[deploy] health"
ssh "$HOST" "curl -fsS http://127.0.0.1:3050/api/health"
echo
echo "[deploy] done: https://river.frasilsynth.com/api/health"
