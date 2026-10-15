# Windows version of run.sh.
# Save demo without game files: downloads a free font, generates the sea and sky of the sea demo,
# the ship of the ship demo and the font of the on-screen log, and starts the engine with the ship
# at sea. Saves go to save/ next to this script and survive restarts.
# Usage: run.ps1 [path to engine.exe] [engine options, e.g. --editor]
#   without the engine path build/default/Debug/engine.exe is taken
$Demo = $PSScriptRoot
. "$Demo/../common/demo.ps1" @args

& "$Demo/../dialog/fetch_assets.ps1" "$Demo/assets"
Invoke-Python "$Demo/../sea/make_sea.py" "$Demo/resource"
Invoke-Python "$Demo/../ship/make_ship.py" "$Demo/resource"
Invoke-Python "$Demo/make_save.py" "$Demo/assets" "$Demo/resource"
# On Linux the engine finds only files whose folder existed when it started, so save/ is made here

Start-Demo -Folders 'resource/sounds', 'resource/ini/aliases', 'save'
