#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
# Build FRASIL with fm1-base (base/, a git submodule).
#   ./build.sh [--release X.Y] [--host | --web]       (the version: VERSION's, unless --release names another)
#   JIELI_TOOLCHAIN  JieLi Linux toolchain (default: ~/.jieli/toolchain)
#   AC79_SDK         JieLi AC79 SDK checkout (default: ~/fw-AC79_AIoT_SDK)
#   PYTHON           a Python with Pillow and fontTools (default: .venv/bin/python)
set -e
cd "$(dirname "$0")"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
PY="${PYTHON:-.venv/bin/python}"
[ -x "$PY" ] || PY=python3
[ -f base/tools/build.py ] || { echo "build.sh: no fm1-base in base/ (git submodule update --init --recursive)"; exit 1; }
"$PY" -c 'import PIL' 2>/dev/null || { echo "build.sh: $PY has no Pillow (python3 -m venv .venv && .venv/bin/pip install Pillow fonttools mido python-rtmidi)"; exit 1; }
# every build carries the version in VERSION (its identity FM-1_47XXYYZZ, its number on the logo's screen), unless
# --release names another
V="$(tr -d ' \n' < VERSION 2>/dev/null || true)"
case " $* " in
*" --release "*) ;;
*) [ -n "$V" ] && set -- --release "$V" "$@" ;;
esac
exec "$PY" base/tools/build.py app "$@"
