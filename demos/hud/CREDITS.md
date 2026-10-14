# Credits

The HUD demo's binary assets are committed in `resource/` and `assets/` and rebuilt by `run.sh` from free sources: `../dialog/fetch_assets.sh` downloads the font, `make_hud.py` draws the textures of the navigator and the ship panel, and the sea, the ship, the island, the particles, the cannonball, the sounds and the music come from the generators of the sea, ship, island, particles, sound and battle demos.

| Asset | Author | License | Source |
|---|---|---|---|
| Font: PT Serif Regular and Bold (log, speeds, compass letters) | ParaType | SIL Open Font License 1.1 (`assets/OFL.txt`) | [google/fonts](https://github.com/google/fonts/tree/main/ofl/ptserif) |
| Tune: "Drunken Sailor" | traditional | public domain | arranged and synthesized by `../sound/make_sound.py` |
| HUD textures | this repository | same as the engine | drawn by `make_hud.py` |
