# Windows version of run.sh.
# glTF demo: models of the Kenney Pirate Kit (source/*.glb, CC0) converted to .gm and .tx by
# tools/gltf2gm and put into a cove by resource/scenes/cove.scene; the script sets the camera, the
# sun and the fog and loads the scene.
# Usage: run.ps1 [path to engine.exe] [engine options, e.g. --editor]
#   without the engine path build/default/Debug/engine.exe is taken
$Demo = $PSScriptRoot
. "$Demo/../common/demo.ps1" @args

Invoke-Python "$Demo/make_assets.py" "$Demo/resource" | Out-Null

Start-Demo -Folders 'resource/sounds', 'resource/ini/aliases'
