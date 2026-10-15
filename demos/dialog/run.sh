#!/bin/bash
# Dialog demo without game files: downloads a free font, generates the sea and sky of the sea demo,
# a pier with a sailor, the dialog window and its fonts, and starts the engine on the pier.
# Usage: run.sh <path to built engine executable> [engine options, e.g. --editor]
set -euo pipefail
ENGINE="$(realpath "$1")"
DIR="$(cd "$(dirname "$0")" && pwd)"

"$DIR/fetch_assets.sh" "$DIR/assets"
python3 -I "$DIR/../sea/make_sea.py" "$DIR/resource"
python3 -I "$DIR/make_dialog.py" "$DIR/assets" "$DIR/resource"
# shaders are copied next to the engine by the build; empty folders are expected by the services
ln -sfn "$(dirname "$ENGINE")/resource/techniques" "$DIR/resource/techniques"
mkdir -p "$DIR/resource/ini/aliases"

cd "$DIR"
exec "$ENGINE" "${@:2}"
