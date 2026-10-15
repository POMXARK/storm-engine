# Windows version of run.sh.
# Scene demo: the models of the model demo and two sound sources (the surf, a bell), put into a scene
# in the editor and saved to resource/scenes/harbour.scene; the script only sets the camera and the sun
# and loads the scene.
# Usage: run.ps1 [path to engine.exe] [engine options, e.g. --editor]
#   without the engine path build/default/Debug/engine.exe is taken
$Demo = $PSScriptRoot
. "$Demo/../common/demo.ps1" @args

# the models and textures are the ones of the model demo, by its generator
Invoke-Python "$Demo/../model/make_models.py" "$Demo/resource"
# the sounds are synthesized by the generator of the sound demo
Invoke-Python "$Demo/make_sounds.py" "$Demo/resource"

Start-Demo -Folders 'resource/sounds', 'resource/ini/aliases'
