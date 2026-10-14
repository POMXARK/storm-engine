#!/bin/bash
# Ship demo without game files: generates the sea and sky of the sea demo, the ship model with its
# textures and the sail parameters, and starts the engine with the ship at sea.
# Usage: run.sh <path to built engine executable>
set -euo pipefail
ENGINE="$(realpath "$1")"
DIR="$(cd "$(dirname "$0")" && pwd)"

python3 -I "$DIR/../sea/make_sea.py" "$DIR/resource"
python3 -I "$DIR/make_ship.py" "$DIR/resource"
# shaders are copied next to the engine by the build; empty folders are expected by the services
ln -sfn "$(dirname "$ENGINE")/resource/techniques" "$DIR/resource/techniques"
mkdir -p "$DIR/resource/sounds" "$DIR/resource/ini/aliases"

cd "$DIR"
exec "$ENGINE"
