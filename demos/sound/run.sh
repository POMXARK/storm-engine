#!/bin/bash
# Sound demo without game files: downloads a free font, generates the sea and sky of the sea demo,
# the ship of the ship demo, and the sounds, the music, the buoy and the log font of this demo,
# then starts the engine with the brig beside a bell buoy.
# Usage: run.sh <path to built engine executable> [engine options, e.g. --editor]
set -euo pipefail
ENGINE="$(realpath "$1")"
DIR="$(cd "$(dirname "$0")" && pwd)"

"$DIR/../dialog/fetch_assets.sh" "$DIR/assets"
python3 -I "$DIR/../sea/make_sea.py" "$DIR/resource"
python3 -I "$DIR/../ship/make_ship.py" "$DIR/resource"
python3 -I "$DIR/make_sound.py" "$DIR/assets" "$DIR/resource"
# shaders are copied next to the engine by the build
ln -sfn "$(dirname "$ENGINE")/resource/techniques" "$DIR/resource/techniques"

cd "$DIR"
exec "$ENGINE" "${@:2}"
