#!/bin/bash
# Character demo without game files: generates a skinned sailor, his skeleton and animations,
# textures and a floor, and starts the engine with him idling or walking around.
# Usage: run.sh <path to built engine executable>
set -euo pipefail
ENGINE="$(realpath "$1")"
DIR="$(cd "$(dirname "$0")" && pwd)"

python3 -I "$DIR/make_character.py" "$DIR/resource"
# shaders are copied next to the engine by the build; empty folders are expected by the services
ln -sfn "$(dirname "$ENGINE")/resource/techniques" "$DIR/resource/techniques"
mkdir -p "$DIR/resource/sounds" "$DIR/resource/ini/aliases"

cd "$DIR"
exec "$ENGINE"
