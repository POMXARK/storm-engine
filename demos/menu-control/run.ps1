$Demo = 'C:\corsairs-remake\engine\demos\menu-control'
. "$Demo/../common/demo.ps1" @args

# the menu pictures and the fonts are the ones the interface demo downloads, this demo
# keeps its own copy of them in its folders
& "$Demo/../interface/fetch_assets.ps1" "$Demo/assets"
Invoke-Python "$Demo/../interface/make_assets.py" "$Demo/assets" "$Demo/resource"

# the engine refuses to start without these folders, an empty one is enough for them
Start-Demo @('resource/sounds', 'resource/ini/aliases')
