#!/bin/sh
# FRASIL River v2: the FM-1's own screens for the Send dialog's drawing (assets/dev-*.png), shot by the simulator.
#   sh design/river-v2/scripts/device-screens.sh        (./build.sh --host first, if app/build/host/frasil_host is missing)
# The play page, the restore question (app.c's "askscreen": ui.c app_backup_ask_screen), STOP FIRST and RESTORED come
# from the simulator as built. NOT RESTORED after FX (or no answer) has no host command, and its first line is longer
# than the pop-up's line buffer (app.c pop.line[3][28] cuts "FX, OR NO ANSWER IN A MINUTE." to "...A MINUT"): it is
# shot from a copy of app/ in a temporary folder, with a "saydeclined" command and the buffer at 32. app/ is untouched.
set -e
cd "$(dirname "$0")/../../.."
HOST=app/build/host/frasil_host
OUT=design/river-v2/assets
W="$(mktemp -d)"
[ -x "$HOST" ] || { echo "no $HOST: ./build.sh --host"; exit 1; }
cat > "$W/play.txt" <<'S'
master 3000
wait 1500
tap SEL
turn K2 2
turn K3 2
tap HOME
tapkey 9 90
wait 1500
tapkey 2 80
wait 6000
shot play.png
S
cat > "$W/pop.txt" <<'S'
wait 600
askscreen
shot ask.png
wait 50
tap EDIT
wait 30
press PLAY
wait 650
release PLAY
wait 50
tapkey 7
wait 100
backup out:playing.frasil
restore out:playing.frasil
wait 50
shot stopfirst.png
tap SEL
wait 50
tapkey 7
wait 30
press PLAY
wait 650
release PLAY
wait 100
backup out:ok.frasil
restore out:ok.frasil
wait 50
shot restored.png
S
"$HOST" "$W/play.txt" "$W" >/dev/null
"$HOST" "$W/pop.txt" "$W" >/dev/null
# NOT RESTORED (FX, or no answer in a minute): a copy of app/, a host command, the line buffer at 32
mkdir -p "$W/copy"
rsync -a --exclude build app/ "$W/copy/app/"
perl -0pi -e 's/    if \(!strcmp\(cmd, "askscreen"\)\) \{/    if (!strcmp(cmd, "saydeclined")) { app_say("RESTORE FROM USB", "NOTHING CHANGED"); return 1; }\n    if (!strcmp(cmd, "askscreen")) {/; s/line\[3\]\[28\]/line[3][32]/' "$W/copy/app/app.c"
PY="${PYTHON:-.venv/bin/python}"
"$PY" base/tools/build.py "$W/copy/app" --host >/dev/null
printf 'wait 600\nsaydeclined\nwait 50\nshot declined.png\n' > "$W/dec.txt"
"$W/copy/app/build/host/frasil_host" "$W/dec.txt" "$W" >/dev/null
cp "$W/play.png" "$OUT/dev-play.png"
cp "$W/ask.png" "$OUT/dev-ask.png"
cp "$W/stopfirst.png" "$OUT/dev-stop-first.png"
cp "$W/restored.png" "$OUT/dev-restored.png"
cp "$W/declined.png" "$OUT/dev-not-restored.png"
rm -rf "$W"
echo "assets/dev-*.png: play, ask, stop-first, restored, not-restored"
