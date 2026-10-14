# 6. Рецепты

Рабочие примеры — демо в `demos/` (каждое запускается `run.sh <engine>`, ассеты генерирует скрипт `make_*.py`, сторонних файлов нет, кроме описанных в `CREDITS.md`) и `tools/smoke-test` (звук + выход). Код демо подробно прокомментирован: рецепт ниже — выжимка, детали — в файле.

| Механика | Демо | Скрипт |
|---|---|---|
| Меню (xinterface, шрифт, звук кнопок) | `demos/interface` | `program/menu.c` |
| Статические модели, свободная камера, солнце | `demos/model` | `program/model.c` |
| Скелетная анимация | `demos/character` | `program/character.c` |
| Локация, персонаж игрока, камера от третьего лица | `demos/location` | `program/location.c` |
| Море, небо, погода | `demos/sea` | `program/sea.c` |
| Корабль: паруса, ход, руль, морская камера | `demos/ship` | `program/ship.c` |

## 6.1 Минимальная игра
```c
#include "storm-engine\layers.h"
void Main()
{
    Trace("hello");                         // → stash/Logs/compile.log
    SetEventHandler("frame", "OnFrame", 0);
}
int n = 0;
void OnFrame() { n++; if (n > 180) ExitProgram(); }
```

## 6.2 Экран меню
1. Контролы — без них GUI не реагирует. Привязка выглядит так: `CreateControl("IUp"); MapControl(id, 38);` и т.д. Обязательно привязать `ILClick` 1, `IRClick` 2, `ITurnH` 256 и `ITurnV` 257 (с `SetControlFlags(id, INVERSE_CONTROL /*4*/)`), `IUp` 38, `IDown` 40, `IAction` 13, `ICancel` 27 (см. `demos/interface/program/menu.c`).
2. Создать GUI:
```c
object GameInterface;
CreateEntity(&GameInterface, "xinterface");
LayerAddObject(4, &GameInterface, -100);   // INTERFACE_EXECUTE
LayerAddObject(5, &GameInterface, -100);   // INTERFACE_REALIZE
SetEventHandler("ievnt_command", "Menu_Command", 0);
SetEventHandler("exitCancel", "Menu_Quit", 0);
SendMessage(&GameInterface, "ls", 45010, "resource\ini\interfaces\mainmenu.ini");
```
3. В `Menu_Command` прочитать `string cmd = GetEventData(); string node = GetEventData();`.
4. Нужные файлы: `interfaces.ini`, `defaultnode.ini`, `pictures.ini`, `mainmenu.ini`, `texts/language.ini` и строки, `fonts.ini`, текстуры `.tx`, `resource/techniques`.

## 6.3 Свой шрифт
- Классический вариант: сгенерировать атлас и `char_X = x,y,w,h`; так делает `demos/interface/make_assets.py`. Затем в `fonts.ini` описать `[interface_normal] Texture=fonts\x.tga, Techniques=InterfaceFont, Height=…, Texture_xsize/ysize=…`, а в `engine.ini` указать `font = interface_normal`.
- BmFont: экспортировать из BMFont в бинарный unicode `.fnt` и в `fonts.ini` указать `file = …`. Регистр пути должен совпадать точно.

## 6.4 Своя текстура
Сохранить A8R8G8B8 (BGRA, 1 mip) с заголовком `<6i 0,w,h,1,21,w*h*4` как `resource/textures/<path>/<name>.tga.tx`, затем ссылаться на неё как `<path>\<name>.tga`. Проще — `write_tx(image, path)` из `demos/common/storm_formats.py` (A8R8G8B8 со всеми mip-уровнями). Посмотреть готовую `.tx` — `demos/common/tx2png.py`. Запись DXT пока не реализована.

## 6.5 Звук
```c
object Sound;
CreateEntity(&Sound, "Sound");
SendMessage(&Sound, "lsll", 77003, "smoke_tone.wav", 4, 1);   // WAV_STEREO, VOLUME_FX
```
Для алиаса нужна секция в `resource/ini/aliases/*.ini`, имя передаётся без `\`.

## 6.6 Модель, камера, свет (`demos/model`)
```c
#define MSG_MODEL_LOAD_GEO 20500
#define MSG_MODEL_SET_POSITION 20506
#define MSG_MODEL_SET_TECHNIQUE 20510
CreateEntity(&Camera, "FREE_CAMERA");              // Camera.position.x/y/z до создания
CreateEntity(&Scene, "Scene");                     // камеры исполняет Scene, а не слои
LayerAddObject(EXECUTE, &Scene, 0);
SendMessage(&Scene, "si", "SetActiveCamera", &Camera);
CreateEntity(&Weather, "weather");                 // солнце = D3D-свет + ambient + туман
Weather.Sun.Color = ...; Weather.Sun.HeightAngle = 0.7; Weather.Fog.Enable = 1; ...
Weather.isDone = "";                               // применяет всё разом
CreateEntity(&model, "modelr");
SendMessage(&model, "ls", MSG_MODEL_LOAD_GEO, "demo\deck");     // resource\models\demo\deck.gm
SendMessage(&model, "ls", MSG_MODEL_SET_TECHNIQUE, "DLightModel"); // без неё свет не действует
LayerAddObject(EXECUTE, &model, 10);
LayerAddObject(REALIZE, &model, 10);
// позиция, затем оси модели vx, vy, vz (model.cpp:323)
SendMessage(&model, "lffffffffffff", MSG_MODEL_SET_POSITION, x, y, z, c, 0.0, -s, 0.0, 1.0, 0.0, s, 0.0, c);
```
FREE_CAMERA читает контролы `FreeCamera_Turn_H/V` (оси мыши 256/257), `FreeCamera_Forward/Backward`.

## 6.7 Скелетная анимация (`demos/character`)
Модель — как в 6.6, но `.gm` скиннинговый (см. 4.2), затем:
```c
SendMessage(&Sailor, "ls", 20501, "demo\sailor");          // MSG_MODEL_LOAD_ANI → resource\animation\demo\sailor.ani
SendMessage(&Sailor, "lls", 20601, 0, "walk");             // MSG_MODEL_PLAY_ACTION: плеер 0, действие из .ani
```
Перемещение модели по кругу делает скрипт в `frame`, скорость совпадает с `Walk speed` из `.ani`, иначе ноги скользят.

## 6.8 Локация и персонаж игрока (`demos/location`)
Порядок важен:
1. `Scene`, `weather` (как в 6.6).
2. `location`: `Yard.id = "yard"; CreateEntity(&Yard, "location");`, затем
   `MSG_LOCATION_MODELSPATH 30140` и `TEXTURESPATH 30141` (`"ls"`, подкаталог в `models\`/`textures\`),
   `MSG_LOCATION_ADD_MODEL 30100` (`"lssl"`: имя, техника, уровень слоя; пятый `l` = 1 включает динамический свет),
   `MSG_LOCATION_SET_CHRS_PATCH 30120` (`"ls"`, `.ptc` без расширения). Локация создаётся **до** персонажей: они регистрируются в ней.
3. `CreateEntity(&Groups, "CharactersGroups")`: персонажи при создании регистрируются в группах (свои, враги).
4. `Sailor.id = "sailor"; CreateEntity(&Sailor, "player");`, `MSG_CHARACTER_SETMODEL 30200` (`"lss"`: модель из `models\characters\`, `.ani`). Набор стояний по умолчанию `idle_1..idle_11`; заменить: `Sailor.actions.idle.i1 = "idle_1"; Sailor.actions = "";`. Поставить: `MSG_CHARACTER_TELEPORT_AY 30211` (`"lffff"`: x, y, z, угол).
5. Камера: `locationcamera`, `MSG_CAMERA_SETTARGET 30500` (`"li"`, &персонаж), `MSG_CAMERA_FOLLOW 30510`, `SetActiveCamera` у Scene.
6. Обработчики, которые тени персонажей вызывают каждый кадр: `EWhr_GetShadowDensity` (возвращает `ref` на `int[2]` — цвета у головы и ног) и `EWhr_GetFogDensity` (float).

Контролы игрока: `ChrForward`, `ChrBackward`, `ChrTurnHL/HR`, `ChrTurnH` (мышь), `ChrRun` (по умолчанию персонаж бежит, клавиша переключает на шаг), `ChrCamTurnV`. Позиция и угол: `MSG_CHARACTER_GETPOS 30202` (`"leee"`), `GETAY 30203` (`"le"`).

## 6.9 Море, небо, погода (`demos/sea`)
```c
#define SEA_REFLECTION 15                           // слой, который море отражает
SetEventHandler("WhrCalcFogColor", "Demo_FogColor", 0);   // ДО неба: оно спрашивает цвет тумана у каждой вершины
CreateEntity(&Sky, "Sky");
Sky.Size = 2048.0;
Sky.techSky = "Sky"; Sky.techSkyBlend = "SkyBlend"; Sky.techSkyAlpha = "skyblend_alpha"; Sky.techSkyFog = "SkyFog";
LayerAddObject(EXECUTE, &Sky, 2); LayerAddObject(REALIZE, &Sky, 3); LayerAddObject(SEA_REFLECTION, &Sky, 1);
CreateEntity(&Sea, "sea");                          // грузит resource\sea\sea0000..0063.tga (4.9)
LayerAddObject(EXECUTE, &Sea, 4); LayerAddObject(REALIZE, &Sea, 65530);   // море рисуется последним
// смена погоды
DeleteAttribute(&Sky, "Dir");
Sky.Dir.d0 = "weather\skies\day\";                  // sky_fr/lf/bk/rt/up.tga.tx
Sky.Dir = "12";                                     // час: несколько d0..dN смешиваются по нему
Sky.isDone = "";
Sea.Sea2.WaterColor = ...; Sea.Sea2.SkyColor = ...; Sea.Sea2.Amp1 = 6.0; ... Sea.isDone = "";
```
- `Demo_FogColor` получает x, y, z вершины (`GetEventData` ×3) и возвращает ARGB.
- Атрибуты `Sea2.*` читаются в `Sea::AttributeChanged` (`sea.cpp`): два слоя волн (`Amp/AnimSpeed/Scale/MoveSpeed 1` — крупная зыбь, `…2` — рябь), `Reflection`, `Transparency`, `Frenel`, `Attenuation`, пена `Foam*` (текстура `weather\sea\pena\pena.tga`), `SimpleSea`, `LodScale`, `GridStep`; `Sea.Fog.*`, `Sea.MaxSeaHeight`.
- Если море зелёное (дефолт `v4SeaColor` 0,255,0), значит `Sea2.*` не применились: см. 3.4.
- Камеру в шторм держать выше гребней (`Amp1` 12 → камера на y = 20).

## 6.10 Корабль: паруса, ход, руль (`demos/ship`)
Модель и паруса генерирует `make_ship.py` (4.10), море и небо — как в 6.9.
```c
int iShipPriorityExecute = 2; int iShipPriorityRealize = 31;   // глобальные: движок читает их по имени
CreateEntity(&Sail, "sail");                        // читает resource\ini\rigging.ini [SAILS], без него — throw
LayerAddObject(EXECUTE, &Sail, iShipPriorityExecute + 1); LayerAddObject(REALIZE, &Sail, iShipPriorityRealize + 1);
LayerAddObject(SAILS_TRACE, &Sail, 10); LayerAddObject(SEA_REFLECTION2, &Sail, 3);
BaseShip.Name = "Brig";                             // models\ships\Brig\Brig.gm
BaseShip.SpeedRate = 12.0; BaseShip.TurnRate = 40.0; BaseShip.Inertia{Acceleration,Braking}{X,Y,Z} = ...;
Player.index = 0; Player.MainCharacter = 1;         // корабль сам читает контролы Ship_TurnLeft/Right, Ship_SailUp/Down
Player.Ship.Speed.z = 1.0;                          // > 0 — старт с поднятыми парусами
SetEventHandler("SeaGetLayers", "Demo_ShipLayers", 0);          // aref ship → SendMessage(ship, "lll", 51015, EXECUTE, REALIZE)
SetEventHandler("Shp_UpdParams", "Demo_ShipParameters", 0);     // "lf": index, доля парусов 0..1, ~раз в секунду
SetEventHandler("GetSailTextureData", "Demo_SailTextures", 0);  // "l" → вернуть ref на объект с normalTex
CreateEntity(&Player, "ship");
SendMessage(&Player, "laa", 50300, &Player, &BaseShip);         // MSG_SHIP_CREATE
SendMessage(&Player, "ls", 20510, "DLightModel");                // иначе корпус не освещается солнцем
// камера
CreateEntity(&SeaCameras, "SEA_CAMERAS"); CreateEntity(&ShipCamera, "SHIP_CAMERA");
ShipCamera.Distance = 0.05; ShipCamera.MinDistance = 25.0; ShipCamera.MaxDistance = 1500.0; ...
SendMessage(&SeaCameras, "lia", 55001, &ShipCamera, &Player);    // AI_CAMERAS_SET_CAMERA
SendMessage(&Scene, "si", "SetActiveCamera", &ShipCamera);
```
- Скорость задаёт скрипт в `Shp_UpdParams`: `Player.Ship.MaxSpeedZ` (узлы, корабль умножает на долю парусов и ветер), `Player.Ship.MaxSpeedY` (поворот, умножается на −1..1 от руля). Движок пишет фактические `Player.ship.speed.x/y/z`.
- Ветер — `Weather.Wind.Angle` (рад) и `Weather.Wind.Speed` (м/с); паруса полностью надуты при 12 м/с.
- Контролы камеры: `ShipCamera_Turn_H/V` (мышь), `ShipCamera_Forward/Backward` (зум). Управление в демо: A/D — руль, W/S — паруса, мышь и Q/E — камера, ←/→ — ветер.
- Некритичные предупреждения в `error.log`: нет `BLOT.TGA`, «no active ShipsLights», «doesn't have fire places» — у демо нет фонарей и мест пожаров.

