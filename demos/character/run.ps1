# Windows version of run.sh.
# Character demo without game files: generates a skinned sailor, his skeleton and animations,
# textures and a floor, and starts the engine with him idling or walking around.
# Usage: run.ps1 [path to engine.exe] [engine options, e.g. --editor]
#   without the engine path build/default/Debug/engine.exe is taken
$Demo = $PSScriptRoot
. "$Demo/../common/demo.ps1" @args

Invoke-Python "$Demo/make_character.py" "$Demo/resource"

Start-Demo -Folders 'resource/sounds', 'resource/ini/aliases'
