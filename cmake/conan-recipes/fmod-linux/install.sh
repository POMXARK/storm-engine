#!/bin/bash
# Packages the FMOD Engine Linux SDK into the local Conan cache under the same reference
# the engine requires on Windows, so `-DSTORM_SOUND_BACKEND=fmod` works on Linux.
# FMOD is proprietary: download "FMOD Engine" 2.02.x for Linux from https://www.fmod.com/download
# Usage: install.sh <path to fmodstudioapi202xxlinux.tar.gz>
set -euo pipefail
RECIPE_DIR="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
tar xzf "$1" -C "$TMP"
export FMOD_SDK="$(ls -d "$TMP"/fmodstudioapi*linux | head -1)"
conan remove "fmod/2.02.05+1@piratesahoy+storm-engine/stable" -c || true
for bt in Debug Release; do
  conan create "$RECIPE_DIR" -s build_type=$bt
done
