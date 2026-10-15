#!/bin/bash
# World map demo without game files: downloads a free font, generates the archipelago, the ships,
# the sea, the clouds and the interface of the WORLDMAP entity, then starts the engine on the map.
# Usage: run.sh <path to built engine executable> [engine options, e.g. --editor]
set -euo pipefail
ENGINE="$(realpath "$1")"
DIR="$(cd "$(dirname "$0")" && pwd)"

"$DIR/../dialog/fetch_assets.sh" "$DIR/assets"
python3 -I "$DIR/make_worldmap.py" "$DIR/assets" "$DIR/resource"
# shaders are copied next to the engine by the build
ln -sfn "$(dirname "$ENGINE")/resource/techniques" "$DIR/resource/techniques"

cd "$DIR"
exec "$ENGINE" "${@:2}"
