# 5. Модули движка (`S/src/libs/`)

| Модуль | Назначение |
|---|---|
| `core` | цикл, компилятор/VM, сущности, слои, события, file service, engine settings, config |
| `util`, `math`, `shared_headers`, `diagnostics`, `config` | утилиты, вектора/матрицы, общие с скриптами заголовки, логи/sentry, LoadConfig |
| `window`, `input`, `pcs_controls` | SDL-окно, ввод, PC-контролы (класс `controls` в engine.ini) |
| `renderer` | DX9 (на Linux — через dxvk-native, см. [08-linux-build.md](08-linux-build.md)), текстуры, техники, шрифты, скриншоты, видео-текстуры |
| `sound`, `sound_service` | звук: алиасы, схемы, 3D, бэкенд miniaudio/FMOD |
| `xinterface` | GUI: узлы, экраны, строки, aviplayer |
| `geometry`, `model`, `animation` | .gm, модели-сущности, скелетная анимация |
| `collide` | столкновения, трассировка лучей |
| `location`, `locator`, `lighter`, `blade`, `blot`, `teleport`, `animals` | сухопутные локации, персонажи, локаторы, освещение, оружие, кровь |
| `sea`, `ship`, `sea_ai`, `sea_cameras`, `sea_operator`, `sea_creatures`, `sea_foam`, `water_rings`, `ball_splash`, `sink_effect`, `sailors` | море, корабли, ИИ, камеры, эффекты, матросы |
| `rigging`, `mast` | паруса, канаты, флаги, мачты |
| `island`, `weather`, `tornado`, `shadow`, `touch` | острова, погода/небо/звёзды/дождь, смерчи, тени, касания кораблей |
| `worldmap` | глобальная карта |
| `battle_interface` | боевой HUD |
| `dialog` | диалоги |
| `particles` | системы частиц (.xps) |
| `script_library` | мелкие нативные функции для скриптов |
| `steam_api` | Steam |
| `editor` | встроенный редактор (`--editor`) |
| `src/apps/configapp` | отдельная утилита настроек |

Корабль (проверено в `demos/ship`, рецепт 6.10): сущность `ship` (корпус, ход, качка; события `SeaGetLayers`, `Shp_UpdParams`, `ShpCreate`, `Ship_StartLoad/EndLoad`), `sail` из `rigging` (паруса, их анимация по ветру из `Weather.Wind.*`), `SEA_CAMERAS` + `SHIP_CAMERA` из `sea_cameras` (камера вокруг корабля, `AI_CAMERAS_SET_CAMERA 55001`). Нужны также `weather`, `sky`, `sea` из 6.9.

Для своей игры достаточно `core`, `renderer`, `xinterface`, `sound_service`, `pcs_controls` (это подтверждает `tools/menu-demo`). Морские и сухопутные модули подключаются, только если скрипт создаёт их сущности.
