# Windows version of run.sh.
# Trigger demo: a zone of a scene made in the editor (resource/scenes/gate.scene) and the script reacting to
# it. A sailor walks by himself between the markers a and b through the trigger "gate"; the script tells the
# scene where he is (CheckSceneTriggers), and as he comes into the zone the guard takes his guard, the crate
# goes away and the bell rings, as he leaves all comes back.
# Usage: run.ps1 [path to engine.exe] [engine options, e.g. --editor]
#   without the engine path build/default/Debug/engine.exe is taken
$Demo = $PSScriptRoot
. "$Demo/../common/demo.ps1" @args

# the yard, the sailor and the pirate with their fight actions are the melee demo's, by its generator
& "$Demo/../dialog/fetch_assets.ps1" "$Demo/../melee/assets"
Invoke-Python "$Demo/../melee/make_melee.py" "$Demo/../melee/assets" "$Demo/resource"
# the crate is the model demo's, the bell the scene demo's
Invoke-Python "$Demo/../model/make_models.py" "$Demo/resource"
Invoke-Python "$Demo/../scene/make_sounds.py" "$Demo/resource"

Start-Demo -Folders 'resource/sounds', 'resource/ini/aliases'
