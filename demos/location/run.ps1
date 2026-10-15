# Windows version of run.sh.
# Location demo without game files: generates a walled yard with its character patch and the
# sailor as a location character, and starts the engine with him walking around the yard.
# Usage: run.ps1 [path to engine.exe] [engine options, e.g. --editor]
#   without the engine path build/default/Debug/engine.exe is taken
$Demo = $PSScriptRoot
. "$Demo/../common/demo.ps1" @args

Invoke-Python "$Demo/make_location.py" "$Demo/resource"

Start-Demo -Folders 'resource/sounds', 'resource/ini/aliases'
