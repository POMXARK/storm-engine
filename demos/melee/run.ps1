# Windows version of run.sh.
# Melee demo without game files: generates the yard, two fencers with their fight actions, the saber
# and the enemy bars, and starts the engine with the sailor facing a pirate.
# Usage: run.ps1 [path to engine.exe] [engine options, e.g. --editor]
#   without the engine path build/default/Debug/engine.exe is taken
$Demo = $PSScriptRoot
. "$Demo/../common/demo.ps1" @args

& "$Demo/../dialog/fetch_assets.ps1" "$Demo/assets"
Invoke-Python "$Demo/make_melee.py" "$Demo/assets" "$Demo/resource"

Start-Demo -Folders 'resource/sounds', 'resource/ini/aliases'
