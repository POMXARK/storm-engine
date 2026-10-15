#!/bin/bash
# glTF demo: models of the Kenney Pirate Kit (source/*.glb, CC0) converted to .gm and .tx by
# tools/gltf2gm and put into a cove by resource/scenes/cove.scene; the script sets the camera, the
# sun and the fog and loads the scene.
# Usage: run.sh <path to built engine executable> [engine options, e.g. --editor]
set -euo pipefail
ENGINE="$(realpath "$1")"
DIR="$(cd "$(dirname "$0")" && pwd)"

python3 -I "$DIR/make_assets.py" "$DIR/resource" > /dev/null
# shaders are copied next to the engine by the build; empty folders are expected by the services
ln -sfn "$(dirname "$ENGINE")/resource/techniques" "$DIR/resource/techniques"
mkdir -p "$DIR/resource/sounds" "$DIR/resource/ini/aliases"

cd "$DIR"
exec "$ENGINE" "${@:2}"
