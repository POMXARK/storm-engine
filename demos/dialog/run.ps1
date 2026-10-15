# Windows version of run.sh.
# Dialog demo without game files: downloads a free font, generates the sea and sky of the sea demo,
# a pier with a sailor, the dialog window and its fonts, and starts the engine on the pier.
# Usage: run.ps1 [path to engine.exe] [engine options, e.g. --editor]
#   without the engine path build/default/Debug/engine.exe is taken
$Demo = $PSScriptRoot
. "$Demo/../common/demo.ps1" @args

& "$Demo/fetch_assets.ps1" "$Demo/assets"
Invoke-Python "$Demo/../sea/make_sea.py" "$Demo/resource"
Invoke-Python "$Demo/make_dialog.py" "$Demo/assets" "$Demo/resource"

Start-Demo -Folders 'resource/ini/aliases'
