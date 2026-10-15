# Windows version of run.sh.
# Particles demo without game files: generates the sea and sky of the sea demo, the particle
# sprites and systems and a rock, and starts the engine with a fire burning on the rock.
# Usage: run.ps1 [path to engine.exe] [engine options, e.g. --editor]
#   without the engine path build/default/Debug/engine.exe is taken
$Demo = $PSScriptRoot
. "$Demo/../common/demo.ps1" @args

Invoke-Python "$Demo/../sea/make_sea.py" "$Demo/resource"
Invoke-Python "$Demo/make_particles.py" "$Demo/resource"

Start-Demo -Folders 'resource/sounds', 'resource/ini/aliases'
