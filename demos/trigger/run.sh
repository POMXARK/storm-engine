#!/bin/bash
# Trigger demo: a zone of a scene made in the editor (resource/scenes/gate.scene) and the script reacting to
# it. A sailor walks by himself between the markers a and b through the trigger "gate"; the script tells the
# scene where he is (CheckSceneTriggers), and as he comes into the zone the guard takes his guard, the crate
# goes away and the bell rings, as he leaves all comes back.
# Usage: run.sh <path to built engine executable> [engine options, e.g. --editor]
set -euo pipefail
ENGINE="$(realpath "$1")"
DIR="$(cd "$(dirname "$0")" && pwd)"

# the yard, the sailor and the pirate with their fight actions are the melee demo's, by its generator
"$DIR/../dialog/fetch_assets.sh" "$DIR/../melee/assets"
python3 -I "$DIR/../melee/make_melee.py" "$DIR/../melee/assets" "$DIR/resource"
# the crate is the model demo's, the bell the scene demo's
python3 -I "$DIR/../model/make_models.py" "$DIR/resource"
python3 -I "$DIR/../scene/make_sounds.py" "$DIR/resource"
# shaders are copied next to the engine by the build; empty folders are expected by the services
ln -sfn "$(dirname "$ENGINE")/resource/techniques" "$DIR/resource/techniques"
mkdir -p "$DIR/resource/sounds" "$DIR/resource/ini/aliases"

cd "$DIR"
exec "$ENGINE" "${@:2}"
