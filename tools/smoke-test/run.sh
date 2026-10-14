#!/bin/bash
# Smoke test without game files: window + DX9 renderer (dxvk) + sound service.
# The script creates the Sound entity, plays a tone, renders SMOKE_FRAMES frames and exits.
# Usage: run.sh <path to built engine executable>
set -euo pipefail
ENGINE="$(realpath "$1")"
DIR="$(cd "$(dirname "$0")" && pwd)"
LOGS="${XDG_DATA_HOME:-$HOME/.local/share}/Akella/Sea Dogs/Logs"

mkdir -p "$DIR/resource/sounds"
[ -f "$DIR/resource/sounds/smoke_tone.wav" ] || python3 -I "$DIR/make_tone.py" "$DIR/resource/sounds/smoke_tone.wav"

cd "$DIR"
# -k: SDL turns SIGTERM into a quit event, which is not processed while the window is inactive
status=0
timeout -k 5 60 "$ENGINE" > "$DIR/engine.out" 2>&1 || status=$?

if [ "$status" -eq 0 ] && grep -q "SMOKE: OK" "$LOGS/compile.log"; then
    echo "smoke test passed"
else
    echo "smoke test FAILED (exit code $status), see $DIR/engine.out and $LOGS" >&2
    exit 1
fi
