# Windows version of run.sh.
# World map demo without game files: downloads a free font, generates the archipelago, the ships,
# the sea, the clouds and the interface of the WORLDMAP entity, then starts the engine on the map.
# Usage: run.ps1 [path to engine.exe] [engine options, e.g. --editor]
#   without the engine path build/default/Debug/engine.exe is taken
$Demo = $PSScriptRoot
. "$Demo/../common/demo.ps1" @args

& "$Demo/../dialog/fetch_assets.ps1" "$Demo/assets"
Invoke-Python "$Demo/make_worldmap.py" "$Demo/assets" "$Demo/resource"

Start-Demo
