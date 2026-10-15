# Windows version of run.sh.
# HUD demo without game files: the battle of the island demo with the battle_interface HUD over it.
# Downloads a free font, generates the sea, the ship, the particles, the sounds, the cannonball and
# the island of the earlier demos and the textures of the navigator and the ship panel, then starts
# the engine.
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
Invoke-Python "$Demo/../island/make_island.py" "$Demo/resource"
Invoke-Python "$Demo/make_hud.py" "$Demo/assets" "$Demo/resource"

Start-Demo
