# AGENTS.md — Storm Engine fork (POMXARK)

Legacy C++ game engine of Sea Dogs / Pirates of the Caribbean / Age of Pirates, forked from
PiratesAhoy/storm-engine. This fork adds a Linux port (dxvk-native), a miniaudio sound backend,
an ImGui editor, scene authoring, script hot reload, glTF import and self-contained demos.

## Read first
- `docs/ru/README.md` — index of the engine guide (Russian). Read the section you need before
  changing code: 01 architecture, 03 scripting VM, 04 file formats, 05 modules, 07 debugging,
  08 Linux build, 09/10 editor.
- `demos/README.md` — every engine mechanic with its demo and status.

## Layout
- `src/libs/<module>` — engine modules (core, renderer, sea, ship, sound_service, editor, ...).
- `src/apps/engine` — the executable.
- `demos/<mechanic>` — one mechanic per demo, no game files needed. Built binary assets are
  committed together with their `make_*.py` generators (`demos/common`).
- `tools/` — converters and `tools/smoke-test`.
- The game itself lives in a separate repo `corsairs-remake` (this engine is its submodule).
  Never put game content or "Corsairs" branding into this repo.

## Build and check
- Linux: `. ../env.sh` then CMake presets `default|debug|release` (`CMakePresets.json`), Conan 2,
  clang. Binary: `build/Debug/engine`.
- Windows: `build.bat` (one step), demos via `run.ps1`; binary `build\default\Debug\engine.exe`.
- After any engine change: `tools/smoke-test/run.sh build/Debug/engine` must print `smoke test passed`.
- After a change in a mechanic: run its demo `demos/<demo>/run.sh build/Debug/engine`.
- Never drive the game with simulated key presses (xdotool): test scenarios go through a script
  `autotest.c` using `PostEvent` with delays.

## Screenshots and video (Windows)
- Use ffmpeg from the shell, not an MCP server (no tool definitions in the context):
  - screenshot: `ffmpeg -y -f gdigrab -i desktop -frames:v 1 shot.png`
  - 10 s video: `ffmpeg -y -f gdigrab -framerate 30 -i desktop -t 10 -c:v libx264 -pix_fmt yuv420p clip.mp4`
  - one window: replace `-i desktop` with `-i title="<window title>"`.
- Start the engine in the background first, capture, then read the PNG to check the result.
- Do not put captures into the repo unless the task asks for it.

## Debugging crashes (Windows)
- The engine is open source and the Debug build has a PDB: never decompile `engine.exe` in Ghidra
  and never parse dumps by hand. Ghidra is only for the original game binaries.
- Read a dump with cdb (Windows SDK, Debugging Tools) and symbols from the build folder:
  `cdb -z crash.dmp -y build\default\Debug -c ".ecxr; kP 30; .frame 0; dv /t /v; q"`
  This gives the exact `file.cpp:line`, arguments and locals of every frame.
- Or reproduce under the debugger: `cdb -g -G -y build\default\Debug build\default\Debug\engine.exe`.
- An AV inside `strlen` on a member array (`char name[256]`) means the object pointer is bad
  (freed or garbage), not the string. Check object lifetime first: who frees it, is the pointer
  still in a container, was the entity/model released before the call.
- Turn logging on before guessing: `spdlog::set_level` in `src/apps/engine/src/main.cpp`.

## Modelling in Blender (blender MCP)
- Plan first: write the parts with exact sizes in metres and positions before any code
  (e.g. "post 0.2 x 0.2 x 2.0 at z=1.0; beam 1.2 x 0.2 x 0.2 at z=1.4").
- Build from primitives (cube, cylinder) plus modifiers (Bevel, Mirror, Array, Boolean, Solidify);
  do not move single vertices by hand. Join parts and apply scale at the end.
- One part per `execute_blender_code` call. After each call print `obj.dimensions` and
  `obj.location` and compare with the plan, then check with `blender_look`; fix before the next part.
- Ready assets (Poly Haven, CC0) are fine; credit them in CREDITS.md.
- Export for the engine as glTF; keep the generating script as `make_<name>.py` next to the asset.

## Rules
- Format with the root `.clang-format`. Keep the style of the surrounding code.
- Prefer the C++ standard library over new custom containers/utilities.
- Only free-licensed assets, credited in CREDITS.md. Do not use j88-yo tools or their outputs.
- A new mechanic = demo in `demos/` + guide update in `docs/ru` in the same commit
  (format -> 04, recipe -> 06, pitfall -> 07 or 03). Delete outdated guide text instead of keeping it.
- Do not invent engine behaviour: check the code (`src/libs/...`) and cite `file.cpp:function`.
- Small, focused changes. Do not reformat or rename code unrelated to the task.

## Git
- Branch: `miniaudio-sound` (on top of `linux-build`; merge, never rebase or force-push).
- Remote `fork` = POMXARK (push here). Remote `origin` = upstream PiratesAhoy: never push, no PRs.
- Commit messages in English:

```
fix: short summary in imperative mood (<= 72 chars)

path/to/file.cpp, other.h:
- what changed — why (the symptom or error it fixes)

Tests:
- smoke test / demo that was run

refs: storm-devs/storm-engine#348
```

## Before finishing a task
1. Build passes. 2. Smoke test passes. 3. The affected demo runs. 4. `docs/ru` updated if behaviour
or formats changed. 5. Report what was verified and what was not.
