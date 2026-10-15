# Engine demos

Small self-contained examples of the engine's basic mechanics. They need no files from any game:
every demo has its own `engine.ini`, scripts and configs, and binary assets (textures, fonts, sounds)
made from free sources. The built assets are committed, so a demo runs as cloned; `run.sh` rebuilds
them with the `make_*.py` generators and `fetch_assets.sh` before every start. Game content does not belong here — the game lives in its own
repository and uses this engine as a submodule.

Each demo is started with `run.sh <path to built engine executable>` from its folder. On Windows: `run.bat` or `run.ps1` next to it (the same steps; without the engine path `build\default\Debug\engine.exe` is taken).
`common/record.sh` records a run to a video with scripted input (`RECORD_AUDIO=1` adds the sound). How the engine works, the formats and a recipe per demo are described (in Russian) in [`docs/ru`](../docs/ru).

| Mechanic | Demo | Status |
|---|---|---|
| Window, DX9 renderer (dxvk on Linux), script VM, sound service | [`tools/smoke-test`](../tools/smoke-test) | done, runs headless-ish and exits with a check |
| Interface (XINTERFACE screens, text buttons, pictures, string collections) | [`interface`](interface) | done |
| Bitmap fonts and localized strings (`fonts.ini`, `language.ini`, `common.ini`) | [`interface`](interface) | done |
| Input: keyboard and mouse controls (`pcs_controls`, `MapControl`) | [`interface`](interface) | done |
| Textures in `.tx` format | [`interface`](interface) | done |
| Static 3D models (`.gm`) with textures and materials, placed and rotated by script | [`model`](model) | done |
| Free camera (`FREE_CAMERA` run by the `Scene` entity) | [`model`](model) | done |
| Dynamic sun light, ambient and fog (`weather` attributes + `DLightModel` technique) | [`model`](model) | done |
| Skeletal animation: skinned `.gm`, `.an` skeleton, `.ani` actions played and switched by script | [`character`](character) | done |
| Locations: model with BSP (traces, shadows), walkable `.ptc` patch, player character controller, following camera, locators from model labels | [`location`](location) | done |
| Sea (animated waves, cube-map sky reflection, foam), skybox, weather presets switched by script | [`sea`](sea) | done |
| Ship: hull, masts and sails from `.gm` labels, sails in the wind, helm and sail controls, ship camera | [`ship`](ship) | done |
| Particles: fire and smoke, cannonball splashes and blasts from `.xps` systems | [`particles`](particles) | done |
| Dialogs: branching answers, a text field, paging, the speaker's name plate | [`dialog`](dialog) | done |
| Save / load: `SaveEngineState`/`LoadEngineState`, `OnSave`/`OnLoad`, rebuilding entities from restored attributes, on-screen log | [`save`](save) | done |
| Music and 3D sound: streamed music with crossfade, looped and one-shot 3D sounds, aliases, the ambient scheme | [`sound`](sound) | done |
| Naval battle: broadsides, cannonballs (`AIBalls`), hits on the hull, sails and masts, shot holes, sinking | [`battle`](battle) | done |
| Enemy ship AI: `SEA_AI` sails the enemy into range, turns its broadside and fires the guns that bear; cannons as model labels | [`ai-ship`](ai-ship) | done |
| Island: land and sea floor models, a depth map with shallows and a reef, ships striking the shore (`touch`), `SEA_AI` steering round the land, balls hitting the slope | [`island`](island) | done |
| HUD: `battle_interface` navigator (island minimap, ships, fire arcs, compass, wind, broadside charge, speeds, icons) and the ship panel (picture, name, crew, hull and sails) | [`hud`](hud) | done |
| Melee: sabers as `BLADE` models on hand and belt labels, draw/sheathe, fast and force attacks, block, parry, hit reactions, death; the NPC `Fight` task, life and energy in scripts, the enemy bars | [`melee`](melee) | done |
| World map: the `worldmap` entity over a generated archipelago — island names, the player's ship on autopilot or the helm, merchants, pursuing pirates and fighting pairs, storms with lightning, the clock with the sky of the day, wind, morale | [`worldmap`](worldmap) | done |
| Scene authoring: models put into a scene in the editor (`--editor`, window Scene), saved to `resource/scenes/*.scene` and loaded by the script with `LoadScene`; Edit/Play, undo, picking in the viewport | [`scene`](scene) | done |
| Characters in the scene: markers placed in the editor where the script puts the player and NPCs (`GetSceneMarker`), animated models of the scene playing an action; the fencers of melee stand on the markers of `yard.scene` | [`melee`](melee) | done |
| glTF import: models from Blender, Kenney or Sketchfab (`.glb`/`.gltf`) converted by `tools/gltf2gm` into `.gm` and `.tx` and put into a cove scene | [`gltf`](gltf) | done |
| Script hot reload: `.c` files edited in the editor (window Assets), Ctrl+S recompiles them while the game runs, globals and event handlers kept; compile errors linked to the line, the old code runs until they are fixed | [`model`](model) | done |
| Scene sounds: sound files played in the editor (window Assets), sound sources put into a scene (`[[sound]]`, looped 3D sound with a volume and distances), the camera hears them | [`scene`](scene) | done |
| Scene triggers: zones put into a scene in the editor (`[[trigger]]`, a cylinder), the script tells who is where (`CheckSceneTriggers`) and gets `SceneTriggerEnter`/`SceneTriggerExit`; the objects of the scene changed from the script (`SceneObjects`: action, show, move); editor keys W/E/F/Esc | [`trigger`](trigger) | done |

## Videos

A recorded run of every demo (`common/record.sh`); click a frame to open the video.

|   |   |   |   |   |
|---|---|---|---|---|
| [![Interface](interface/media/interface.jpg)](interface/media/interface.mp4)<br>Interface | [![Models](model/media/model.jpg)](model/media/model.mp4)<br>Models | [![Character](character/media/character.jpg)](character/media/character.mp4)<br>Character | [![Location](location/media/location.jpg)](location/media/location.mp4)<br>Location | [![Sea](sea/media/sea.jpg)](sea/media/sea.mp4)<br>Sea |
| [![Ship](ship/media/ship.jpg)](ship/media/ship.mp4)<br>Ship | [![Particles](particles/media/particles.jpg)](particles/media/particles.mp4)<br>Particles | [![Dialog](dialog/media/dialog.jpg)](dialog/media/dialog.mp4)<br>Dialog | [![Save / load](save/media/save.jpg)](save/media/save.mp4)<br>Save / load | [![Sound](sound/media/sound.jpg)](sound/media/sound.mp4)<br>Sound |
| [![Battle](battle/media/battle.jpg)](battle/media/battle.mp4)<br>Battle | [![Enemy ship](ai-ship/media/ai-ship.jpg)](ai-ship/media/ai-ship.mp4)<br>Enemy ship | [![Island](island/media/island.jpg)](island/media/island.mp4)<br>Island | [![HUD](hud/media/hud.jpg)](hud/media/hud.mp4)<br>HUD | [![Melee](melee/media/melee.jpg)](melee/media/melee.mp4)<br>Melee |
| [![World map](worldmap/media/worldmap.jpg)](worldmap/media/worldmap.mp4)<br>World map | [![Scene](scene/media/scene.jpg)](scene/media/scene.mp4)<br>Scene | [![glTF import](gltf/media/gltf.jpg)](gltf/media/gltf.mp4)<br>glTF import | [![Characters in the scene](melee/media/melee-editor.jpg)](melee/media/melee-editor.mp4)<br>Characters in the scene | [![Script hot reload](model/media/model-reload.jpg)](model/media/model-reload.mp4)<br>Script hot reload |
| [![Scene sounds](scene/media/scene-sound.jpg)](scene/media/scene-sound.mp4)<br>Scene sounds | [![Scene triggers](trigger/media/trigger.jpg)](trigger/media/trigger.mp4)<br>Scene triggers |   |   |   |
