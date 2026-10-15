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
| Частицы: огонь, дым, всплески, взрывы | `demos/particles` | `program/particles.c` |
| Диалог: ветвление, поле ввода, листание страниц | `demos/dialog` | `program/dialog.c` |
| Сохранение и загрузка | `demos/save` | `program/save.c` |
| Музыка, 3D-звук, алиасы, фоновая схема | `demos/sound` | `program/sound.c` |
| Морской бой: бортовые залпы, ядра, попадания, потопление | `demos/battle` | `program/battle.c` |
| ИИ-противник: SEA_AI ведёт корабль и стреляет бортами | `demos/ai-ship` | `program/ai_ship.c` |

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
Для алиаса нужна секция в `resource/ini/aliases/*.ini`, имя передаётся без `\`. Музыка, 3D и схемы — 6.14.

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
- Камера на цели: `ShipCamera.Target = sti(Enemy.index)` — камера плавно заходит за корму со стороны, противоположной цели, и встаёт на `ShipCamera.TargetOffset` радиан (по умолчанию 0.3) в сторону от линии на цель: точно на линии свой корабль закрывает цель. `-1` выключает. Пока мышь крутит камеру, наведение ждёт. Скорость наведения — `ShipCamera.TargetInertia` (по умолчанию 1.5 в секунду). Цель ищется среди сущностей `ship` по атрибуту `index` персонажа.
- Зона прицела: `Player.Ship.Cannons.Aim = 1` — `AIShipCannonController` рисует на воде сектор того борта, который смотрит от камеры: от борта до дальности выстрела по `BALL_SPEED` и наибольшему углу ствола, с затуханием к краю и яркой кромкой. Белый — борт заряжен, золотой — враг в секторе, красный — идёт перезарядка. `0` прячет. В игре он держится, пока зажата ПКМ (`MapKey("Sea_Aim", 2, 0)`, отпускание ловится событием `Control Deactivation`). Вершины ставятся на `SEA_BASE::WaveXZ` + 0,5 м, техника `AimZone` (`techniques/ai/aimzone.fx`) — с Z-тестом без записи в Z: корпус закрывает зону, зона не закрывает воду.
- Контролы камеры: `ShipCamera_Turn_H/V` (мышь), `ShipCamera_Forward/Backward` (зум). Управление в демо: A/D — руль, W/S — паруса, мышь и Q/E — камера, ←/→ — ветер.
- Некритичные предупреждения в `error.log`: нет `BLOT.TGA`, «no active ShipsLights», «doesn't have fire places» — у демо нет фонарей и мест пожаров.

## 6.11 Частицы (`demos/particles`)
Атлас, `.xps` и скалу генерирует `make_particles.py` (4.11), море и небо — как в 6.9.
```c
CreateEntity(&Particles, "particles");             // системы — все resource\particles\*.xps, загружены при старте
// "lsffffffl": имя .xps, позиция, углы, время жизни в мс (0 — до удаления)
SendMessage(&Particles, "lsffffffl", 10000, "splash", x, 0.0, z, 0.0, 0.0, 0.0, 0);   // PS_CREATE
// бесконечную систему запоминаем, чтобы потом удалить
SendMessage(&Particles, "l", 10011);               // PS_CREATIONCAPTURE_BEG
SendMessage(&Particles, "lsffffffl", 10000, "campfire", 0.0, 2.5, 0.0, 0.0, 0.0, 0.0, 0);
SendMessage(&Particles, "l", 10012);               // PS_CREATIONCAPTURE_END
SendMessage(&Particles, "l", 10013);               // PS_CLEAR_CAPTURED — удалить захваченные
```
- Неповторяющийся эмиттер (`Looping` = false) работает `Life time` секунд, а система удаляется сама, когда умрёт последняя частица — для всплесков и взрывов ничего удалять не нужно.
- `PS_DELETE 10007` читает указатель как Long — на 64-битной сборке ненадёжен, используйте захват.
- Управление в демо: мышь и W/S — камера, 1 — всплеск ядра, 2 — взрыв, F — потушить/зажечь огонь.

## 6.12 Диалог (`demos/dialog`)
Окно, шрифты (PT Serif, `write_fonts`), пирс и матроса генерирует `make_dialog.py` (4.12), море и небо — как в 6.9.
```c
MapKey("DlgUp", 38); MapKey("DlgDown", 40);        // выбор ответа
MapKey("DlgAction", 32); MapKey("DlgAction2", 13);  // подтвердить (в поле ввода пробел печатается, работает Enter)
MapKey("DlgScrollUp", 33); MapKey("DlgScrollDown", 34);
MapKey("DlgCancel", 27);                            // событие "DialogCancel"
SetEventHandler("DialogEvent", "Dialog_Event", 0);

Sailor.name = "Том"; Sailor.lastname = "Бакстер";   // имя на табличке
Dialog.Text = "Эй, моряк! Как тебя звать?";         // "\n" в тексте — новая страница
Dialog.Links.l1 = "Джек Хоук";                       // сначала текст ответа,
Dialog.Links.l1.go = "name";                         // потом узел (иначе Assert)
Dialog.Links.l1.edit = "string";                     // ответ — поле ввода, введённое — в Dialog.value
CreateEntity(&Dialog, "dialog");
SendMessage(&Dialog, "lii", 1, &Sailor, &Sailor);   // собеседник: имя из name + lastname
LayerAddObject(REALIZE, &Dialog, -256);

void Dialog_Event() { Dialog_Node(Dialog.CurrentNode); }  // заменить Text и Links
// закрыть — не из обработчика самого диалога: PostEvent("Demo_DialogClose", 1) → DeleteClass(&Dialog)
```
- Ответы показываются в порядке добавления; перед новым узлом `DeleteAttribute(&Dialog, "Links")`.
- Пустой `go` или тот же `go`, что у прошлого выбранного ответа, движок считает сломанным деревом и шлёт `EmergencyDialogExit`. Повторяющийся узел («А ещё что?») чередует два имени: `rumour` / `rumour_more`.
- Курсор поля ввода стоит после подсказки: Backspace стирает её, ←/→/Delete работают; ширина ограничена окном.
- Управление в демо: ↑/↓ — ответ, Space/Enter/ЛКМ — выбрать, PgUp/PgDn — листать, Esc — закрыть диалог (ещё раз — выйти), E — заговорить снова.

## 6.13 Сохранение и загрузка (`demos/save`)
Корабль из 6.10, шрифт экранного лога (`ILogAndActions`) генерирует `make_save.py`. `run.sh` создаёт каталог `save/` до старта движка.
```c
string savefile_info = "";                 // попадёт в заголовок файла (4.13)
float sailState = 1.0;                     // состояние внутри сущности копируем в переменную

void Main()    { /* начальные значения в атрибутах */ CreateScene(); }
void OnSave()  { SendMessage(&Player, "le", MSG_SHIP_GET_SAIL_STATE, &sailState); }
void OnLoad()  { CreateScene(); SendMessage(&Player, "lf", MSG_SHIP_SET_SAIL_STATE, sailState); }

SaveEngineState("save\ship.sav");         // синхронно, внутри вызывает OnSave
LoadEngineState("save\ship.sav");         // в следующем кадре: сущности удалены, переменные восстановлены, OnLoad
```
- `CreateScene` общая для старта и загрузки: создаёт сущности по тем атрибутам, что уже лежат в переменных, и заново ставит `SetEventHandler`.
- Атрибут, присвоенный **до** `CreateEntity` (или восстановленный загрузкой), сущность может не увидеть: `weather` берёт ветер только в `AttributeChanged`. Поэтому после создания — `Weather.Wind.Angle = stf(Weather.Wind.Angle);` иначе в сущности мусор, море и паруса рисуются кусками.
- Корабль ставится по `Player.ship.pos`/`ship.ang` при `MSG_SHIP_CREATE`, ship.cpp обновляет эти атрибуты каждый кадр — сохранять отдельно не нужно.
- Есть ли файл: `FINDFILESINTODIRECTORY` с атрибутами `dir`, `mask` заполняет `filelist.id0...` при создании.
- Экранный лог: `ScreenLog.Log.font/left/up/width/height/color/speed/color_speed`, `CreateEntity(&ScreenLog, "ILogAndActions")`, в слои EXECUTE и REALIZE, `SendMessage(&ScreenLog, "lll", 45022, 0, 1)`, строка — `SendMessage(&ScreenLog, "lls", 45020, 0, text)`.
- Управление в демо: F5 — сохранить, F9 — загрузить, A/D — руль, W/S — паруса, ←/→ — ветер, мышь — камера, Esc — выход. Сохранение переживает перезапуск.

## 6.14 Музыка и 3D-звук (`demos/sound`)
Корабль из 6.10 идёт мимо буя с колоколом. Все звуки и обе аранжировки «Drunken Sailor» синтезирует `make_sound.py`; он же пишет алиасы, `sound_scheme.ini` (4.7) и модель буя.
```c
CreateEntity(&Sound, "Sound");
SendMessage(&Sound, "lfff", 77021, 1.0, 0.6, 1.0);                  // громкость: эффекты, музыка, речь

// зацикленный 3D-звук в точке: тип PCM_3D 1, VOLUME_FX 1, cache, looped, cached, fade in, x, y, z, min, max
bell = SendMessage(&Sound, "lsllllllfffff", 77003, "demo\bell.wav", 1, 1, false, true, false, 0, 0.0, 2.0, 70.0, 15.0, 260.0);
// алиас: файл и дистанции из resource\ini\aliases\demo.ini, задаётся только позиция
SendMessage(&Sound, "lsllllllfff", 77003, "cannon", 1, 1, false, false, false, 0, x, 3.0, z);
// музыка: MP3_STEREO 2, VOLUME_MUSIC 2, looped, наплыв 3 с, пауза между повторами, громкость
SendMessage(&Sound, "lslllllllf", 77003, "music\calm.wav", 2, 2, false, true, false, 3000, 0, 1.0);
SendMessage(&Sound, "ls", 77014, "sea_day");                         // фоновая схема
SendMessage(&Sound, "lll", 77004, waves, 2000);                      // остановить по id с затуханием
```
- Слушатель — камера, позицию берёт сам сервис; для движущегося источника — `SendMessage(&Sound, "llfff", 77007, id, 3, x, y, z)` каждый кадр.
- Новый музыкальный трек вытесняет прошлый с кроссфейдом; вернувшийся трек продолжает с места остановки.
- Схема играет `ch`-звуки (можно алиасы) со случайной паузой; зацикленный фон надёжнее запускать скриптом, `ch_loop` после `SCHEME_RESET` не останавливается.
- Управление в демо: Space — выстрел, M — смена музыки, ↑/↓ — громкость музыки, G — фон вкл/выкл, A/D — руль, W/S — паруса, мышь — камера, Esc — выход.

## 6.15 Морской бой (`demos/battle`)
Бриг из 6.10 и второй бриг-мишень. Q/E — залп левым/правым бортом, R — новая мишень на траверзе. Спрайт ядра, пробоины и звуки попаданий генерирует `make_battle.py`, частицы `explosion`/`splash` — генератор 6.11, выстрел — алиас `cannon` из 6.14.
```c
CreateEntity(&AISea, "SEA_AI");                       // без него AIBalls рисует через нулевой рендер
CreateEntity(&AIBalls, "AIBalls");
LayerAddObject(EXECUTE, &AIBalls, 10); LayerAddObject(REALIZE, &AIBalls, 10);
AIBalls.Texture = "demo\ball.tga"; AIBalls.SubTexX = 1; AIBalls.SubTexY = 1; AIBalls.SpeedMultiply = 1.0;
AIBalls.Balls.Balls.Size = 0.5; ...                   // тип ядер "Balls" (4.10.1)
AIBalls.isDone = "";

// одна пушка: дуло на борту, повёрнутое курсом корабля
AIBalls.Type = "Balls"; AIBalls.CharacterIndex = Player.index;
AIBalls.x = x; AIBalls.y = y; AIBalls.z = z; AIBalls.SpdV0 = 80.0;
AIBalls.Ang = 0.055;                                  // возвышение: ~90 м до воды
AIBalls.Dir = heading + side * PI * 0.5;              // side −1 — левый борт, 1 — правый
AIBalls.HeightMultiply = 1.0; AIBalls.SizeMultiply = 1.0; AIBalls.TimeSpeedMultiply = 1.0; AIBalls.Event = "";
AIBalls.add = "";

SetEventHandler("Shp_HullHit", "Demo_HullHit", 0);    // "illffflf": взрыв, звук, счёт корпуса
SetEventHandler("Ball_WaterHit", "Demo_WaterHit", 0); // "lfff": всплеск
SetEventHandler("ShpMastDamage", "Demo_MastDamage", 0); // float Demo_MastDamage() — вернуть новый урон
SendMessage(&Target, "l", 50316);                     // MSG_SHIP_SET_DEAD: корпус на нуле — тонет
```
- Корпус и мачты должны иметь BSP (`write_gm(..., bsp=True)`, 4.2), иначе ядра пролетают насквозь.
- Свои ядра корабль пропускает по `index` персонажа, поэтому у каждого корабля он свой.
- Пушки залпа стреляют по очереди через `GUN_INTERVAL` из обработчика `frame`; разброс по курсу и возвышению — `frnd()`.
- Мишень — не главный персонаж, контролы не читает; паруса стоят, скорость задаёт `MaxSpeedZ` в `Shp_UpdParams`. R удаляет её `DeleteClass` и создаёт заново.
- Ядра летят низко, поэтому мачты почти не задевают; обработчик `ShpMastDamage` всё равно нужен (иначе движок читает ответ из пустоты).
- Управление: Q/E — залпы, R — новая мишень, A/D — руль, W/S — паруса, мышь — камера, Esc — выход.

## 6.16 ИИ-противник (`demos/ai-ship`)
Бриг игрока и вражеский бриг под управлением `SEA_AI`: враг сам подходит на `AttackDistance`, разворачивается бортом и стреляет. Space — огонь бортами игрока, которые достают врага, R — новый бой. Ассеты — генераторы 6.10, 6.11, 6.14 и `make_battle.py` из 6.15; у модели брига по 6 пушек-меток на борт (4.10.2).
```c
CreateEntity(&AISea, "SEA_AI");                        // до AIBalls: он берёт рендер у SEA_AI
LayerAddObject(EXECUTE, &AISea, 0); LayerAddObject(REALIZE, &AISea, 0);
// ... AIBalls как в 6.15

BaseShip.Cannons.Borts.cannonl.FireDir = PI * 1.5;     // борта = группы меток пушек в модели
BaseShip.Cannons.Borts.cannonl.FireZone = 1.4;
BaseShip.Cannons.Borts.cannonl.FireAngMin = -0.1; BaseShip.Cannons.Borts.cannonl.FireAngMax = 0.2;  // дальность ~250 м
// ... то же для cannonr с FireDir = PI * 0.5

Player.index = 0; Player.SeaAI.Group.Name = "PlayerGroup"; Player.MainCharacter = 1;
Player.Ship.Cannons.SpeedV0 = 80.0; Player.Ship.Cannons.Borts = ""; Player.Ship.HP = 100;
Enemy.index = 1;  Enemy.SeaAI.Group.Name = "EnemyGroup"; Enemy.Ship.SeaAI.Init.AttackDistance = 110.0;
// ... остальное как у игрока

SendMessage(&AISea, "lsa", 51022, "EnemyGroup", &Enemy);             // GROUP_SET_COMMANDER
SendMessage(&AISea, "lsffff", 51010, "EnemyGroup", x, 0.0, z, ay);   // GROUP_SET_XYZ_AY
CreateEntity(&Enemy, "ship");
SendMessage(&AISea, "liaa", 51001, &Enemy, &Enemy, &BaseShip);       // ADD_SHIP
// ... то же для игрока
AISea.isDone = "";
SendMessage(&AISea, "lllaa", 51011, 1, 0, &Enemy, &Player);          // SHIP_SET_TASK: ATTACK, PRIMARY

SendMessage(&AISea, "lal", 51002, &Player, 1);                       // CANNON_FIRE: огонь игрока
SendMessage(&AISea, "la", 51026, &Enemy);                            // CHARACTER_DEAD: корабль тонет
```
- События с ответом (обработчик **обязан** вернуть значение): `GetRelationEvent` "ll" → int (1 — друг, 2 — нейтрал, 3 — враг), `Shp_GetCurBallsNum` "a" → int (ядер в трюме), `CannGRTime` "a" → float (время перезарядки), `CannGFTime` "a" → float (задержка между пушками), `CannGFHeight` "aa" → float (высота прицела над водой), `CannLoad` "a" → int, `ShpGetBortFireDelta` "afff" → float (разброс).
- `CannFire` "afffffffff" — выстрел одной пушки: персонаж, дуло x/y/z, `SpeedV0`, курс, возвышение, `dirY`, `MaxFireDistance`, `RawAng`. Скрипт создаёт ядро `AIBalls` (6.15) с этими значениями. `Ship_Bort_Fire` "lisffffff" приходит в начале залпа борта синхронно внутри сообщения огня, поэтому по счётчику залпов скрипт узнаёт, что ни один борт не достаёт («No loaded side bears»). `Shp_BortReloaded` "as" — борт перезаряжен.
- Предел хода и поворота врага скрипт задаёт как обычно — `MaxSpeedZ`/`MaxSpeedY` в `Shp_UpdParams` (6.10); SEA_AI выбирает курс и ход в этих пределах.
- R: `DeleteClass(&AISea)` удаляет группы, `AIShip` и сущности кораблей. Новый бой создавать в следующем кадре (`PostEvent("Demo_StartBattle", 1)`): деструктор SEA_AI срабатывает в конце кадра и чистит статические списки кораблей вместе с уже добавленными новыми (07). После `CreateEntity` заново снова отправить `AI_MESSAGE_SET_CAMERAS_ATTRIBUTE` (Helper.Uninit обнулил камеры) и `AI_CAMERAS_SET_CAMERA`.
- Управление: A/D — руль, W/S — паруса, мышь — камера, Space — огонь, R — новый бой, Esc — выход.

## 6.17 Остров, мель и риф (`demos/island`)
Бой из 6.16, а между бригами — остров: враг стартует за ним и обходит его, чтобы атаковать. У берега мель, на юго-востоке под водой риф. Корабль, который выходит на мель, получает удар и урон, ядра бьются о склон. Ассеты — генераторы 6.16 и `make_island.py`: модель, дно, текстуры, карта глубин и граф обхода строятся из одной функции высот (4.14).
```c
#define MSG_ISLAND_LOAD_GEO 50100

// до SEA_AI: AIHelper ищет остров при создании SEA_AI
CreateEntity(&Island, "island");
Island.LightingPath = "";                      // без готовых цветов вершин
Island.dynamicLightsOn = 0;                    // свет запечён в текстуру
Island.Technique = "DLightModel";              // техника для суши и дна (атрибут добавлен для демо)
Island.FogDensity = 0.0015;                    // свой туман острова
Island.ImmersionDistance = 3000.0; Island.ImmersionDepth = 25.0;  // вдали остров чуть опускается
SendMessage(&Island, "lsss", MSG_ISLAND_LOAD_GEO, "islands", "islands\isle", "isle");
LayerAddObject(REALIZE, &Island, 4);           // до моря: море ложится поверх отмели и дна

// ... SEA_AI, AIBalls как в 6.16
// перед кораблями каждого боя: touch запоминает корабли при создании
CreateEntity(&Touch, "touch");
LayerAddObject(EXECUTE, &Touch, 1);
Touch.CollisionDepth = -4.0;                   // корпус упирается там, где дно мельче 4 м
// ... AddShip игрока и врага

SetEventHandler("Shp_ShipIslColl", "Demo_ShipIslandCollision", 0);  // "llfflfff"
SetEventHandler("Shp2Strand", "Demo_Shallows", 0);                  // "ll"
SetEventHandler("Ball_IslandHit", "Demo_IslandHit", 0);             // "lfff"
```
- Урон от удара — `(1 − slide)·power·k`: в оригинале k = 3. `Shp_ShipIslColl` приходит каждый кадр, пока корпус трётся о дно, а ветер прижимает корабль под парусами к берегу, и толчки доходят до 3–5 узлов. Поэтому демо засчитывает только удар после отрыва: флаг `Ship.Grounded` снимает `Demo_GroundClear`, и только если 1.5 с не было ни одного касания (`Ship.Touched`). Толчки слабее 1 узла не считаются вовсе. Без этого прижатый корабль тонет за пару минут.
- `Shp2Strand` переключается 0/1 на каждой волне, пока киль над краем мели. Предупреждение лотового демо выводит не чаще раза в 5 с.
- Старты кораблей ставить на воду глубже −14: SEA_AI отклоняет мелкие места (`isCanPlace`).
- Ядро летит по дуге и проходит над низким берегом. `Ball_IslandHit` приходит, только если трасса полёта задела модель: склон холма или промах ниже цели. С `OutsideCamera` залп игрока уходит лишь тогда, когда враг в секторе борта и в пределах дальности, а остров стрельбу не запрещает.
- R: `DeleteClass(&Touch)` вместе с `SEA_AI`. Остров живёт всю сцену.
- Управление как в 6.16. Камера корабля стартует перед носом (`vAng.y = 0`), поэтому остров по курсу виден, если развернуть её мышью.

## 6.18 Боевой HUD (`demos/hud`)
Бой у острова из 6.17, поверх него — `battle_interface`. Справа сверху навигатор: мини-карта острова с врагом, сектора и дальность бортов, компас и ветер (повёрнуты курсом вверх), заряд бортов по ободу (зелёный — заряжен, жёлтый — перезарядка, красный — повреждён), скорость ветра и корабля в узлах, иконки ядер, парусов и ветра. Слева снизу панель корабля: картинка, имя, экипаж, HP и SP. Текстуры рисует `make_hud.py`.
```c
#define BI_IN_CREATE_SHIP 46011
#define BI_IN_DEAD_SHIP 46009

// атрибуты — до CreateEntity: Init читает их один раз
BattleInterface.MainChrIndex = sti(Player.index);
BattleInterface.ShowNavigator = 1;
BattleInterface.ShowCommands = 1;              // без него нет и панели корабля
BattleInterface.navigation.navigatorWidth = 220; // ... rightPos/topPos — угол на экране
BattleInterface.navigation.mapRadius = 77;     // круг карты внутри рамки, px
BattleInterface.navigation.horizontRadius = 400; // сколько метров вмещает карта
BattleInterface.navigation.leftChargeBegAngle = 220;  // дуга борта, градусы по часовой от носа
BattleInterface.navigation.leftChargeEndAngle = 320;
BattleInterface.navigation.chargeTexture = "hud\icons.tga";
BattleInterface.navigation.chargeTextureGreed = "4,3";  // сетка картинок в текстуре: столбцы, строки
BattleInterface.navigation.chargePos = "-58,128";       // смещение от центра навигатора
BattleInterface.ShipIcon.iconoffset1 = "84,512";        // центр панели первого корабля
BattleInterface.ShipIcon.shipclassiconsize = "0,0";     // не рисовать класс
BattleInterface.CommandList.ActiveIcon_Size = "0,0";    // и пустую иконку меню
CreateEntity(&BattleInterface, "battle_interface");
LayerAddObject(EXECUTE, &BattleInterface, 65534);
LayerAddObject(REALIZE, &BattleInterface, 65534);

// после кораблей: индекс, персонаж, базовый корабль, свой ли, отношение (0 враг, 1 друг, 2 нейтрал)
SendMessage(&BattleInterface, "llaall", BI_IN_CREATE_SHIP, sti(Player.index), &Player, &BaseShip, 1, 1);
SendMessage(&BattleInterface, "llaall", BI_IN_CREATE_SHIP, sti(Enemy.index), &Enemy, &BaseShip, 0, 0);

int hudIcons[5];                               // заряд, паруса, ветер, порох, порох кончился
ref Hud_GetCurrentCharge() { hudIcons[1] = ...; return &hudIcons; }
SetEventHandler("GetCurrentCharge", "Hud_GetCurrentCharge", 0);
```
- HP и SP панель делит на `BaseShip.HP/SP`. Урон парусам приходит событием `ShpSailDamage` "lfffl" (корабль, точка, владелец ядра) — `Ship.SP` уменьшает скрипт.
- При потоплении — `BI_IN_DEAD_SHIP`: на карте корабль становится серым.
- Рамка «идёт бой» (`battleborder`) включается, когда враг рядом. Её значения по умолчанию рассчитаны на 1024×768 и белые, поэтому рамку стоит задать под своё окно:
  ```c
  BattleInterface.battleborder.used = 1;
  BattleInterface.battleborder.texture = "hud\border.tga"; // цвет из текстуры, альфа = текстура × color
  BattleInterface.battleborder.color1 = argb(200,255,255,255); // пульс между color1 и color2
  BattleInterface.battleborder.color2 = argb(60,255,255,255);
  BattleInterface.battleborder.extpos = "0,0,800,600";        // внешний край, v текстуры = 0
  BattleInterface.battleborder.intpos1 = "24,24,776,576";     // внутренний край в двух фазах пульса
  BattleInterface.battleborder.intpos2 = "14,14,786,586";
  BattleInterface.battleborder.speed = 1.2;                   // ×0.001 за мс
  ```
- R: сначала `DeleteClass(&BattleInterface)`, потом `SEA_AI`. Список кораблей HUD хранит указатели на сущности `ship`, а `SEA_AI` их удаляет. После нового боя HUD создаётся заново.
- Z/X — контролы `MiniMapZoomIn/Out`: масштаб карты меняется, пока клавиша нажата (`scaleStep` за кадр, от `minScale` до `maxScale`).
- Скорость печатается только числом (`%.1f`), подпись к ней — забота текстуры. Debug-клавиша K прячет HUD.
- Лог (6.15) стоит сузить: навигатор занимает правый верхний угол.
- `hudIcons[i]` — номер ячейки атласа, слева направо и сверху вниз. В атласе демо 4×3: ядро, три состояния парусов, три силы ветра, книппель (7), картечь (8).

### Типы зарядов
`AIBalls` знает несколько типов ядер. Тип выбирается перед выстрелом, а при попадании видно, какой тип прилетел:
```c
AIBalls.Balls.Knippels.SubTexIndex = 0;      // кадр текстуры ядра
AIBalls.Balls.Knippels.GoodIndex = 1;        // число, которое вернёт CurrentBallType
AIBalls.Balls.Knippels.Size = 0.65;
AIBalls.Balls.Knippels.Weight = 1.0;

// в обработчике события CannFire, до AIBalls.add = ""
AIBalls.Type = "Knippels";

// в Shp_HullHit и ShpSailDamage: AIBalls::Execute ставит тип перед трассировкой ядер этого типа
int charge = sti(AIBalls.CurrentBallType);
```
- Скорость ядра и дальность борта задаёт `character.Ship.Cannons.SpeedV0` (м/с). Дальность растёт с квадратом скорости, по ней же рисуется зона прицела. Лёгкому заряду — меньшую скорость.
- Высоту, в которую целят пушки, возвращает обработчик `Sea_FireHeight` (метры над ватерлинией цели). На 1.5 м ядра почти не задевают паруса: книппелям нужно 6 м и выше, а попадание книппелем в корпус стоит тоже засчитать парусам.

