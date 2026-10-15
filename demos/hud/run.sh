#!/bin/bash
# HUD demo without game files: the battle of the island demo with the battle_interface HUD over it.
# Downloads a free font, generates the sea, the ship, the particles, the sounds, the cannonball and
# the island of the earlier demos and the textures of the navigator and the ship panel, then starts
# the engine.
# Usage: run.sh <path to built engine executable> [engine options, e.g. --editor]
set -euo pipefail
ENGINE="$(realpath "$1")"
DIR="$(cd "$(dirname "$0")" && pwd)"

"$DIR/../dialog/fetch_assets.sh" "$DIR/assets"
python3 -I "$DIR/../sea/make_sea.py" "$DIR/resource"
python3 -I "$DIR/../ship/make_ship.py" "$DIR/resource"
python3 -I "$DIR/../particles/make_particles.py" "$DIR/resource"
python3 -I "$DIR/../sound/make_sound.py" "$DIR/assets" "$DIR/resource"
python3 -I "$DIR/../battle/make_battle.py" "$DIR/resource"
python3 -I "$DIR/../island/make_island.py" "$DIR/resource"
python3 -I "$DIR/make_hud.py" "$DIR/assets" "$DIR/resource"
# shaders are copied next to the engine by the build
ln -sfn "$(dirname "$ENGINE")/resource/techniques" "$DIR/resource/techniques"

cd "$DIR"
exec "$ENGINE" "${@:2}"
