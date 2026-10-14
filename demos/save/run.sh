#!/bin/bash
# Save demo without game files: downloads a free font, generates the sea and sky of the sea demo,
# the ship of the ship demo and the font of the on-screen log, and starts the engine with the ship
# at sea. Saves go to save/ next to this script and survive restarts.
# Usage: run.sh <path to built engine executable>
set -euo pipefail
ENGINE="$(realpath "$1")"
DIR="$(cd "$(dirname "$0")" && pwd)"

"$DIR/../dialog/fetch_assets.sh" "$DIR/assets"
python3 -I "$DIR/../sea/make_sea.py" "$DIR/resource"
python3 -I "$DIR/../ship/make_ship.py" "$DIR/resource"
python3 -I "$DIR/make_save.py" "$DIR/assets" "$DIR/resource"
# shaders are copied next to the engine by the build; empty folders are expected by the services.
# On Linux the engine finds only files whose folder existed when it started, so save/ is made here
ln -sfn "$(dirname "$ENGINE")/resource/techniques" "$DIR/resource/techniques"
mkdir -p "$DIR/resource/sounds" "$DIR/resource/ini/aliases" "$DIR/save"

cd "$DIR"
exec "$ENGINE"
