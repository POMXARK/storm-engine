# Engine demos

Small self-contained examples of the engine's basic mechanics. They need no files from any game:
every demo has its own `engine.ini`, scripts and configs, and builds binary assets (textures, fonts,
sounds) from free sources at run time. Game content does not belong here — the game lives in its own
repository and uses this engine as a submodule.

Each demo is started with `run.sh <path to built engine executable>` from its folder.

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
| Animated character (`.ani`), locations | — | planned |
| Sea, sky, weather | — | planned |
| Ship sailing and ship controls | — | planned |
| Particles and effects | — | planned |
| Dialogs | — | planned |
| Save / load | — | planned |
| Music and 3D sound | — | planned |
