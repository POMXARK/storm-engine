#!/bin/bash
# Downloads the free assets used by the dialog demo (licenses in CREDITS.md).
# Usage: fetch_assets.sh <assets dir>
set -euo pipefail
DIR="$1"
mkdir -p "$DIR"
FONTS=https://github.com/google/fonts/raw/main/ofl/ptserif
for FILE in PT_Serif-Web-Regular.ttf PT_Serif-Web-Bold.ttf OFL.txt; do
    [ -f "$DIR/$FILE" ] || curl -sfL -o "$DIR/$FILE" "$FONTS/$FILE"
done
