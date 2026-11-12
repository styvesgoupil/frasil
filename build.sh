#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# Build FRASIL with fm1-base (base/, a git submodule).
#   ./build.sh [--release X.Y] [--host | --web]       (the version: VERSION's, unless --release names another)
#   --web also makes stream.wasm (tools/stream_wasm.sh: FRASIL STREAM's engine, app/build/stream/)
#   JIELI_TOOLCHAIN  JieLi Linux toolchain (default: ~/.jieli/toolchain)
#   AC79_SDK         JieLi AC79 SDK checkout (default: ~/fw-AC79_AIoT_SDK)
#   PYTHON           a Python with Pillow and fontTools (default: .venv/bin/python)
#   FRASIL_BASE_ANY  1: build with base/ off this commit's pointer (refused otherwise)
set -e
cd "$(dirname "$0")"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
PY="${PYTHON:-.venv/bin/python}"
[ -x "$PY" ] || PY=python3
[ -f base/tools/build.py ] || { echo "build.sh: no fm1-base in base/ (git submodule update --init --recursive)"; exit 1; }
"$PY" -c 'import PIL' 2>/dev/null || { echo "build.sh: $PY has no Pillow (python3 -m venv .venv && .venv/bin/pip install Pillow fonttools mido python-rtmidi)"; exit 1; }
# base/ must be the fm1-base this commit names: 3.0.2-b2 to b4 were built (and flashed) with a stale base/ (92d6581, its
# pointer said 442c6e3). FRASIL_BASE_ANY=1 builds anyway (fm1-base's own work); its package is refused for Styves's
# FM-1 (tools/live-guard.sh --package: the sidecar says so). No git here (a build box's copy): not checked.
if git rev-parse --git-dir >/dev/null 2>&1 && [ -e base/.git ]; then
    ptr="$(git rev-parse HEAD:base 2>/dev/null || true)"
    at="$(git -C base rev-parse HEAD 2>/dev/null || true)"
    if [ -n "$ptr" ] && [ -n "$at" ] && [ "$ptr" != "$at" ] && [ "${FRASIL_BASE_ANY:-}" != 1 ]; then
        echo "build.sh: base/ is at $(echo "$at" | cut -c1-7) but this commit's pointer says $(echo "$ptr" | cut -c1-7): \
a stale submodule (git submodule update --init base), or commit the pointer; FRASIL_BASE_ANY=1 to build anyway"
        exit 1
    fi
fi
# every build carries the version in VERSION (its identity FM-1_47XXYYZZ, its number on the logo's screen), unless
# --release names another
V="$(tr -d ' \n' < VERSION 2>/dev/null || true)"
# an SMK-37 build (FM1B_BOARD=smk37 FM1B_MODEL=elite, card #111) is a dev build: its model's identity ("SMK-37 Elite_",
# 13 characters) leaves no room for FRASIL's 47 and a version within 20: SMK-37 Elite_4700, until an SMK-37 release
[ "${FM1B_BOARD:-}" = smk37 ] && V=""
case " $* " in
*" --release "*) ;;
*) [ -n "$V" ] && set -- --release "$V" "$@" ;;
esac
case " $* " in
*" --web "*)
    "$PY" base/tools/build.py app "$@"
    exec tools/stream_wasm.sh ;;
esac
case " $* " in
*" --host "*) exec "$PY" base/tools/build.py app "$@" ;;
esac
# a package: PACKAGE.fwsc.json beside it, the commits it was built from (tools/live-guard.sh --sidecar), which a flash
# to Styves's FM-1 checks (tools/fm1-flash.sh, the runner's FLASH: tools/live-guard.sh --package)
mark="app/build/.build-start"
mkdir -p app/build && : > "$mark"
"$PY" base/tools/build.py app "$@"
if git rev-parse --git-dir >/dev/null 2>&1 && command -v python3 >/dev/null 2>&1; then
    find app/build -maxdepth 1 -name '*.fwsc' -newer "$mark" | while IFS= read -r pkg; do
        tools/live-guard.sh --sidecar "$pkg" || echo "build.sh: no sidecar for $pkg (a flash to Styves's FM-1 will refuse it)"
    done
fi
rm -f "$mark"
