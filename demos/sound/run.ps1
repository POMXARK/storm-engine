# Windows version of run.sh.
# Sound demo without game files: downloads a free font, generates the sea and sky of the sea demo,
# the ship of the ship demo, and the sounds, the music, the buoy and the log font of this demo,
# then starts the engine with the brig beside a bell buoy.
# Usage: run.ps1 [path to engine.exe] [engine options, e.g. --editor]
#   without the engine path build/default/Debug/engine.exe is taken
$Demo = $PSScriptRoot
. "$Demo/../common/demo.ps1" @args

& "$Demo/../dialog/fetch_assets.ps1" "$Demo/assets"
Invoke-Python "$Demo/../sea/make_sea.py" "$Demo/resource"
Invoke-Python "$Demo/../ship/make_ship.py" "$Demo/resource"
Invoke-Python "$Demo/make_sound.py" "$Demo/assets" "$Demo/resource"

Start-Demo
