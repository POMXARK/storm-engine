#!/bin/bash
# Interface demo without game files: downloads free assets, converts them to engine formats
# (.tx textures, fonts.ini) and starts the engine with the XINTERFACE main menu.
# Usage: run.sh <path to built engine executable>
set -euo pipefail
ENGINE="$(realpath "$1")"
DIR="$(cd "$(dirname "$0")" && pwd)"

"$DIR/fetch_assets.sh" "$DIR/assets"
python3 -I "$DIR/make_assets.py" "$DIR/assets" "$DIR/resource"
# shaders are copied next to the engine by the build; empty folders are expected by the services
ln -sfn "$(dirname "$ENGINE")/resource/techniques" "$DIR/resource/techniques"
mkdir -p "$DIR/resource/sounds" "$DIR/resource/ini/aliases"

cd "$DIR"
exec "$ENGINE"
