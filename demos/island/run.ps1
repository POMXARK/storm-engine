# Windows version of run.sh.
# Island demo without game files: downloads a free font, generates the island, its sea floor and
# depth map, the sea and sky of the sea demo, the ship of the ship demo, the particles, the sounds
# and the cannonball of the particles, sound and battle demos, then starts the engine with two brigs
# on either side of the island, the second steered round it by SEA_AI.
# Usage: run.ps1 [path to engine.exe] [engine options, e.g. --editor]
#   without the engine path build/default/Debug/engine.exe is taken
$Demo = $PSScriptRoot
. "$Demo/../common/demo.ps1" @args

& "$Demo/../dialog/fetch_assets.ps1" "$Demo/assets"
Invoke-Python "$Demo/../sea/make_sea.py" "$Demo/resource"
Invoke-Python "$Demo/../ship/make_ship.py" "$Demo/resource"
Invoke-Python "$Demo/../particles/make_particles.py" "$Demo/resource"
Invoke-Python "$Demo/../sound/make_sound.py" "$Demo/assets" "$Demo/resource"
Invoke-Python "$Demo/../battle/make_battle.py" "$Demo/resource"
Invoke-Python "$Demo/make_island.py" "$Demo/resource"

Start-Demo
