#!/bin/sh
# The menu control autotest: the main menu of the interface demo driven only by messages.
# Run it with: sh ./run.sh [engine path]
DIR=$(dirname "$0")
ENGINE=${1:-"$DIR/../../build/default/Debug/engine"}
[ -e ./run.sh ] || {
    echo "run.sh: this demo is not meant to run on Linux: the engine it needs is not built there"
    exit 1
}
if ! ../interface/fetch_assets.sh "$DIR/assets"; then
    echo "menu-control: fetching its assets failed, starting from what ./assets already has"
fi
if ! ../interface/make_assets.py "$DIR/assets" "$DIR/resource"; then
    echo "menu-control: regenerating its asset folders failed, starting from what ./resource already has"
fi
[ -f "$DIR/resource/models.txt" ] && { echo "menu-control: models.txt is a Windows loader path, this demo cannot run on Linux"; exit 1; }
# the engine reads the technique file names through the filesystem: on Linux they resolve
# directly, on Windows they need a link from resource/techniques to the shipped DX11 files
[ "$(uname -s)" = "Linux" ] || ln -sfn "$(dirname "$ENGINE")/resource/techniques" "$DIR/resource/techniques"
# the folders the engine refuses to start without, empty ones are enough for them
mkdir -p "$DIR/resource/sounds" "$DIR/resource/ini/aliases"
cd "$DIR"
exec "$ENGINE" "${@:2}"
