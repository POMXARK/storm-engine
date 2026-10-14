#!/bin/bash
# Particles demo without game files: generates the sea and sky of the sea demo, the particle
# sprites and systems and a rock, and starts the engine with a fire burning on the rock.
# Usage: run.sh <path to built engine executable>
set -euo pipefail
ENGINE="$(realpath "$1")"
DIR="$(cd "$(dirname "$0")" && pwd)"

python3 -I "$DIR/../sea/make_sea.py" "$DIR/resource"
python3 -I "$DIR/make_particles.py" "$DIR/resource"
# shaders are copied next to the engine by the build; empty folders are expected by the services
ln -sfn "$(dirname "$ENGINE")/resource/techniques" "$DIR/resource/techniques"
mkdir -p "$DIR/resource/sounds" "$DIR/resource/ini/aliases"

cd "$DIR"
exec "$ENGINE"
