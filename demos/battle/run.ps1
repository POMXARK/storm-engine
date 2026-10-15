# Windows version of run.sh.
# Battle demo without game files: downloads a free font, generates the sea and sky of the sea demo,
# the ship of the ship demo, the particles of the particles demo, the sounds of the sound demo and
# the cannonball, the shot holes and the hit sounds of this demo, then starts the engine with two
# brigs at sea.
# Usage: run.ps1 [path to engine.exe] [engine options, e.g. --editor]
#   without the engine path build/default/Debug/engine.exe is taken
$Demo = $PSScriptRoot
. "$Demo/../common/demo.ps1" @args

& "$Demo/../dialog/fetch_assets.ps1" "$Demo/assets"
Invoke-Python "$Demo/../sea/make_sea.py" "$Demo/resource"
Invoke-Python "$Demo/../ship/make_ship.py" "$Demo/resource"
Invoke-Python "$Demo/../particles/make_particles.py" "$Demo/resource"
Invoke-Python "$Demo/../sound/make_sound.py" "$Demo/assets" "$Demo/resource"
Invoke-Python "$Demo/make_battle.py" "$Demo/resource"

Start-Demo
