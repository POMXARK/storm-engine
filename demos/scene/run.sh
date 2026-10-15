#!/bin/bash
# Scene demo: the models of the model demo, put into a scene in the editor and saved to
# resource/scenes/harbour.scene; the script only sets the camera and the sun and loads the scene.
# Usage: run.sh <path to built engine executable> [engine options, e.g. --editor]
set -euo pipefail
ENGINE="$(realpath "$1")"
DIR="$(cd "$(dirname "$0")" && pwd)"

# the models and textures are the ones of the model demo, by its generator
python3 -I "$DIR/../model/make_models.py" "$DIR/resource"
# shaders are copied next to the engine by the build; empty folders are expected by the services
ln -sfn "$(dirname "$ENGINE")/resource/techniques" "$DIR/resource/techniques"
mkdir -p "$DIR/resource/sounds" "$DIR/resource/ini/aliases"

cd "$DIR"
exec "$ENGINE" "${@:2}"
