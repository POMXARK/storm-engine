#!/bin/bash
# Enemy ship demo without game files: downloads a free font, generates the sea and sky of the sea
# demo, the ship of the ship demo, the particles of the particles demo, the sounds of the sound demo
# and the cannonball, the shot holes and the hit sounds of the battle demo, then starts the engine
# with two brigs at sea, the second steered and fought by SEA_AI.
# Usage: run.sh <path to built engine executable>
set -euo pipefail
ENGINE="$(realpath "$1")"
DIR="$(cd "$(dirname "$0")" && pwd)"

"$DIR/../dialog/fetch_assets.sh" "$DIR/assets"
python3 -I "$DIR/../sea/make_sea.py" "$DIR/resource"
python3 -I "$DIR/../ship/make_ship.py" "$DIR/resource"
python3 -I "$DIR/../particles/make_particles.py" "$DIR/resource"
python3 -I "$DIR/../sound/make_sound.py" "$DIR/assets" "$DIR/resource"
python3 -I "$DIR/../battle/make_battle.py" "$DIR/resource"
# shaders are copied next to the engine by the build
ln -sfn "$(dirname "$ENGINE")/resource/techniques" "$DIR/resource/techniques"

cd "$DIR"
exec "$ENGINE"
