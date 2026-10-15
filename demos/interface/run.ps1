# Windows version of run.sh.
# Interface demo without game files: downloads free assets, converts them to engine formats
# (.tx textures, fonts.ini) and starts the engine with the XINTERFACE main menu.
# Usage: run.ps1 [path to engine.exe] [engine options, e.g. --editor]
#   without the engine path build/default/Debug/engine.exe is taken
$Demo = $PSScriptRoot
. "$Demo/../common/demo.ps1" @args

& "$Demo/fetch_assets.ps1" "$Demo/assets"
Invoke-Python "$Demo/make_assets.py" "$Demo/assets" "$Demo/resource"

Start-Demo -Folders 'resource/sounds', 'resource/ini/aliases'
