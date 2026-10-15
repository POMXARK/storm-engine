#!/bin/bash
# Sea demo without game files: generates the skyboxes, the wave heightmaps and the foam, and
# starts the engine over the open sea.
# Usage: run.sh <path to built engine executable> [engine options, e.g. --editor]
set -euo pipefail
ENGINE="$(realpath "$1")"
DIR="$(cd "$(dirname "$0")" && pwd)"

python3 -I "$DIR/make_sea.py" "$DIR/resource"
# shaders are copied next to the engine by the build; empty folders are expected by the services
ln -sfn "$(dirname "$ENGINE")/resource/techniques" "$DIR/resource/techniques"
mkdir -p "$DIR/resource/sounds" "$DIR/resource/ini/aliases"

cd "$DIR"
exec "$ENGINE" "${@:2}"
