# Windows version of run.sh.
# Sea demo without game files: generates the skyboxes, the wave heightmaps and the foam, and
# starts the engine over the open sea.
# Usage: run.ps1 [path to engine.exe] [engine options, e.g. --editor]
#   without the engine path build/default/Debug/engine.exe is taken
$Demo = $PSScriptRoot
. "$Demo/../common/demo.ps1" @args

Invoke-Python "$Demo/make_sea.py" "$Demo/resource"

Start-Demo -Folders 'resource/sounds', 'resource/ini/aliases'
