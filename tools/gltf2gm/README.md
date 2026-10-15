# gltf2gm

Converts glTF 2.0 models (`.glb`, `.gltf`) from Blender, Kenney, Sketchfab and others into Storm Engine static models: a `.gm` per file and a `.tx` per texture. Needs Python 3 and Pillow, not Blender.

```bash
# Blender: File → Export → glTF 2.0 (.glb)
python3 tools/gltf2gm/gltf2gm.py ship.glb props/ --out game/resource --folder harbour
# -> resource/models/harbour/ship.gm, loaded as "harbour\ship"
```

| Option | What |
|---|---|
| `--out` | the `resource` folder of the game |
| `--folder` | subfolder of `models/` and `textures/` (default `imported`) |
| `--scale` | for models not in metres |
| `--no-bsp` | skip the BSP tree (traces and editor picking miss the model) |
| `--single-sided` | do not duplicate the faces of `doubleSided` materials |

Node transforms are baked, x is mirrored (glTF is right-handed, D3D left-handed), the base colour texture and factor become the texture. Skins and animations are not read: for characters use `tools/blender-gm-export`.

Details (Russian): `docs/ru/04-formats.md` §4.17, `docs/ru/06-recipes.md` §6.22. Example: `demos/gltf`.
