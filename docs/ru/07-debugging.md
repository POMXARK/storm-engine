# 7. Отладка

| Что | Где / как |
|---|---|
| Логи | stash `Logs/`: `compile.log` (Trace, предупреждения компилятора), `error.log` (ошибки VM, SendMessage), `script_stack` (compiler.cpp:139), `system.log` (логгер по умолчанию, main.cpp:24) |
| Отключить логи | `logs = 0` в engine.ini |
| Отладочная информация VM | `[script] debuginfo = 1`, `runtimelog`, `codefiles` |
| Кэш скриптов | `[script] cache_mode = 0` при отладке, иначе правки могут подхватываться из stash/`Cache` |
| Сущности | F7 → `DumpEntitiesInfo` |
| Время | R ×10, Y ×0.2 |
| Память | Shift+F1 → статистика mimalloc |
| Рендер | `show_fps`, `show_exinfo`, `texture_log`, `safe_render` |
| Атрибуты | `DumpAttributes(&obj)` |
| Точка останова | `Breakpoint()` **[поведение на Linux не проверено]** |
| Headless-проверка | `tools/smoke-test/run.sh`: `timeout` + поиск маркера в `compile.log` ([08-linux-build.md](08-linux-build.md) §8) |
| Нажатия в тестах | `xdotool keydown K; sleep 0.15; xdotool keyup K`: мгновенное нажатие движок может пропустить. Клик мышью по кнопке меню через xdotool не срабатывает, Enter — да |
| Видео запуска | `demos/common/record.sh` (ниже) |

Частые проблемы:
- «Failed to compile program» → текст ошибки в `error.log` (`COMPILE ERROR in файл(строка)`). До фикса `demos/battle` логи после этой ошибки оставались пустыми: они сбрасываются периодически, а движок сразу падал; теперь `core_impl.cpp` сбрасывает их перед окном ошибки. Окно SDL открывается в дочернем процессе `engine` и держит движок, пока не нажать OK. Частая причина — несуществующая функция: случайное float — `frnd()` (0..1), не `frand`.
- Глобальная переменная, инициализированная через `#define` (`int hull = TARGET_HULL;`), остаётся 0 → присвоить в `Main()`.
- Ядра пролетают сквозь корабль → у модели нет BSP (4.2). `AIBalls` без сущности `SEA_AI` падает на нулевом рендере, корабль без обработчика `ShpMastDamage` — при попадании в мачту (4.10.1).
- Сцена рисуется кусками (чёрные/красные плоскости, нет неба и парусов), хотя ресурсы те же → атрибут задан до `CreateEntity` или восстановлен загрузкой, и сущность его не прочитала (`weather` берёт значения только в `AttributeChanged`). Присвоить заново после создания (6.13).
- Файл не находится на Linux → проверить, что путь начинается с `program`/`resource`/`save` и что каталог существовал до старта движка (2.2).
- Текстура не грузится → проверить, что есть `.tx` (raw TGA не читается на Linux) и что формат входит в список поддерживаемых (4.1).
- `Wrong script version` → не объявлен `iScriptVersion = 54128` при `target_version = teho`.
- Нет GUI-реакции → не привязаны контролы `I*` или слои 4/5.
- Окно не обновляется → без фокуса нужен `run_in_background = 1`.
- Море зелёное, атрибуты после `isDone` не применились, Trace после присваивания молчит → обработчик движка, вызванный из присваивания атрибута, оборвал функцию (3.4; исправлено в e1c878b).
- `Incorrect message data: expected 'l', but got 'f'` (или `got ''`) в `error.log` → в формате `SendMessage` букв меньше или больше, чем аргументов; первая `l` — код сообщения.
- `CreateMessage: Invalid Data` в `error.log` на `SendMessage(&AISea, ...)` → формат не учитывает код сообщения или тип аргумента (`a` — ссылка на атрибуты, `i` — сущность): для `SHIP_SET_TASK` это `"lllaa"`, для `ADD_SHIP` — `"liaa"`.
- Падение в `AIBalls::AttributeChanged` → `AIBalls` создан раньше `SEA_AI`, `AIHelper::pRS` ещё нулевой (6.16).
- Assert в `sea_ai` при добавлении корабля → нет `Ship.Cannons.Borts` у персонажа или у группы не назначен командир (`GROUP_SET_COMMANDER` до `ADD_SHIP`). Без обработчиков событий-ответов SEA_AI (6.16) движок читает значение из пустоты.
- Space не стреляет («No loaded side bears») → ни один борт не смотрит на врага (пушки только по бортам, нужно развернуться, 4.10.2) или враг дальше дальности пушек: она растёт с `FireAngMax` и `SpeedV0`. После залпа ИИ может отойти намного дальше `AttackDistance`.
- Прицел SEA_AI ведёт себя случайно → `bCameraOutside` в `ai_ship_camera_controller.h` был не инициализирован (UB); теперь по умолчанию `false`, демо ставит `AICameras.Crosshair.OutsideCamera = 1`.
- Звук, запущенный с fade in, не слышен, а остановленный с fade out играет дальше → движок до фикса `demos/sound` обрабатывал фейды только у музыки (4.7).
- Остров не виден, хотя модель загружена и `ISLAND::Realize` вызывается. Сначала развернуть камеру: `SHIP_CAMERA` стартует перед носом корабля и смотрит против курса. Если и тогда пусто — грани смотрят вниз (4.2): у сетки высот обход должен быть по часовой при взгляде сверху. Снизу такая модель видна, и это хороший тест: поднять копию модели над камерой.
- Остров изменился, а мель и столкновения остались старыми → движок читает `foam\islands\<имя>.tga.zap`, а не tga (4.14). Удалить zap.
- Корабли проходят сквозь сушу, SEA_AI не обходит остров → `touch` или `SEA_AI` созданы раньше острова либо `touch` — после кораблей. Нужен порядок остров → SEA_AI → touch → корабли (05).
- `CreateMessage: Invalid Data` на `BI_IN_CREATE_SHIP` → в исходнике формат записан как `"laallll"` без кода сообщения. Скрипту нужен `"llaall"` (код, индекс, персонаж, базовый корабль, свой, отношение) и, по желанию, ещё `ll` — цвет и «передаваемый».
- Навигатор есть, панели корабля нет → `BattleInterface.ShowCommands = 0`: панель рисует меню команд (05).
- На HUD пустые белые квадраты → у `BIShipIcon` и меню команд размеры по умолчанию 128×128 и 64×64. Ненужным частям (`shipclassiconsize`, `CommandList.ActiveIcon_Size`) ставить `"0,0"`.
- Белые полосы 20 px сверху и слева во время боя → это рамка `battleborder` (`BIBorderInfo`) со значениями по умолчанию: белая, под 1024×768. Задать `extpos`/`intpos`, текстуру и цвета под своё окно (6.18).
- По R (новый бой) падение в `TOUCH::IsSinked`, `pShip == nullptr` → `DeleteClass(&AISea)` и создание нового боя выполнены в одном кадре. Деструктор SEA_AI запускается в конце кадра и чистит статические `AIGroup::AIGroups`/`AIShip::AIShips` вместе с новыми кораблями, а их сущности удаляет. Новый бой создавать в следующем кадре: `PostEvent("Demo_StartBattle", 1)` (6.16).
- `terminate ... mast.ini file not found!` после попадания ядра → мачта сломалась и стала сущностью `mast`, а та требует `resource/ini/mast.ini`. Все ключи (секция `[MAST]`) имеют значения по умолчанию, достаточно пустого файла. Его пишет `make_ship.py`.
- `battle_interface` хранит указатели на корабли: удалять его до `SEA_AI` и создавать после новых кораблей (6.18).
- Новые `Sea.Sea2.Amp1`/`Amp2` не меняют волны → сущность `sea` запоминает их, но применяет только при записи `Sea.MaxSeaHeight` (`sea.cpp`, `AttributeChanged`). Там же амплитуды уменьшаются так, чтобы их сумма не превышала эту высоту. После амплитуд всегда задавать `MaxSeaHeight`. Иначе в гавани остаются волны открытого моря и перехлёстывают пирс.
- Новый `.fx` в `src/techniques` движок не находит → техники копируются в `build/.../resource/techniques` по `file(GLOB_RECURSE)` при конфигурации cmake: после добавления файла выполнить `cmake build` заново, обычная сборка его не увидит.
- Оверлей без Z-теста не виден на море → море рисуется в `REALIZE` с приоритетом 65530 и закрашивает всё, что нарисовано раньше. Сущность, которая рисует поверх воды (`SEA_AI` с зоной прицела), ставить после моря: `LayerAddObject(REALIZE, &AISea, 65531)`.
- Плоскость на воде с Z-тестом пропадает → гребни волн выше её. Высоту брать из `SEA_BASE::WaveXZ(x, z)` в каждой вершине, с запасом ~0,5 м, и бить сетку мельче шага волн.
- Урон парусам, записанный скриптом в `Ship.SP`, пропадает: паруса снова 100 % → `SAIL` пересчитывает `Ship.SP` по дыркам своих мешей (`sail.cpp`, ~425) всякий раз, когда корабль ставит или убирает паруса. Урон хранить в своём атрибуте (`Ship.Torn`) и каждый кадр писать `Ship.SP = 100 - Torn` (6.18).
- Книппели или картечь почти не попадают, хотя ядра попадают → обработчик `CannGFHeight` целит выше корпуса: с 150–200 м снаряд, нацеленный на 6 м, проходит над палубой и редко задевает парус (0 из 48). Целить в 2.5–3 м, а попадание книппелем в корпус засчитывать парусам.
- Корабль под ИИ держится в 150 м от цели, хотя `Ship.SeaAI.Init.AttackDistance` задан → SEA_AI читает его один раз, в `AIShip` при добавлении корабля (`ai_ship.cpp:90`): задавать до `ship`/`isDone`.
- Персонаж достаёт саблю, но не бьёт → обработчик `ChrAttackAction` не вернул int 1 (или его нет): атака отменяется молча (6.19).
- Удары анимируются, но урона нет → в действии атаки нет события `event = "Attack", 50%` (4.3) или персонажи не враги по `CharactersGroups` (`SetRelation ... "enemy"`).
- `not set force attack action: "attack_force_3"` / `not set block(parry) action: "parry_3"` в system.log, хотя в `actions` по одному имени → движок выбирал выпад и парирование из четырёх зон независимо от длины списка. Исправлено в этом демо: лишние зоны отключаются по `numAttackForce`/`numParry` (`character.cpp`, `Attack`/`Parry`).
- Сабля в руке смотрит не туда или висит в стороне от кисти → метка на кости скиннинговой модели хранится сопряжённой зеркалом (4.2); проверить направление клинка расчётом позы (`sailor_fight.self_check`).
- Нет `Undeclared identifier: DeleteEntity` → такой функции в скриптах нет; сущность удаляет `DeleteClass(&obj)`.
- Тучи на глобальной карте фиолетовые или красные → не загрузился вершинный шейдер техники. Движок ищет его как `<fx-файл>_<функция>.vso` (`worldmap.fx` + `compile vs_1_1 main()` → `worldmap_main.vso`), а в репозитории лежал `worldmap_shader.vso`. Без шейдера на экран идёт сырой цвет вершины: в красном — яркость, в зелёном — флаг молнии, в синем — размер тучи. Переименовано. Кроме того, на Linux `wdm_clouds.cpp` отдавал шейдеру 2 строки `cloudMatrix` из 3, и цвет вспышки молнии (c6) был мусором.
- `COMPILE ERROR ... No matching ')'` на выражении вида `a && (b || c)` — в `if` и в инициализации (`bool x = a && (b || c);`). Компилятор скриптов не разбирает скобку после `&&`. Функция с такой строкой компилируется с ошибкой и падает при вызове (`function '...' stack error`), а последствия видны далеко от неё. Например, `Sea_AddShip` не дописал корабль, и BATTLE_INTERFACE упал в `CheckSeaState` на `pShip == nullptr`. Логи при этом пустые (см. 7.2). Лечение: `bool x = b || c; if (!a) { x = false; }`. Перед запуском проверить: `grep -n '&& (' program/*.c`.
- Тест демо убивает движок, не дождавшись окна → `run.sh` перед стартом генерирует ассеты (у `worldmap` ~55 с), и `timeout` съедает это время. В тесте запускать движок напрямую, без генерации, и искать окно по ширине, а не по первому найденному (первым находится служебное окно 46×30).
- Надпись острова на глобальной карте не видна → `labels.*.heightView` меньше высоты камеры (`wdmCameraY`) или метка `labels` в `islands.gm` вне кадра.
- Правка скрипта через Ctrl+S не меняет поведение → изменено начальное значение глобальной переменной или код в `Main()`: при перезагрузке (3.4.1) они не выполняются. Вынести значение в функцию.
- Горячие клавиши ImGui (Ctrl+S, Ctrl+A) не работают при русской раскладке → бэкенд SDL2 ImGui 1.90 берёт клавишу по keycode, а у кириллицы он вне ASCII. `EngineEditor::ProcessEvent` подставляет латинскую клавишу по scancode.
- Источник звука сцены слышен одинаково со всех сторон → файл стерео: 3D-звук (`PCM_3D`) панорамируется только в моно. `make_sounds.py` демо пишет моно.
- Звук сцены слышен не оттуда, где стоит персонаж → слушатель — камера: `SoundService::RunStart` каждый кадр берёт его из матрицы вида рендера. Звуки сцены играют и в Edit, со свободной камерой редактора.
- `SoundGetPosition` всегда возвращает 0 → не реализован в `SoundService`; полоса прогресса в Assets считается по длительности из заголовка WAV и времени с нажатия Play. `SoundRelease` — пустышка, звук останавливает `SoundStop(id)` (id 0 — все звуки).
- `SceneTriggerEnter` не приходит → модуль сам не следит за персонажем: скрипт должен звать `CheckSceneTriggers(who, x, y, z)` каждый тик. События ставятся в очередь (`PostEvent`) и приходят после функции, которая вызвала проверку, а не внутри неё (6.25).
- `native function 'SceneObjectShow' not declared` или функция библиотеки молча не работает → библиотека привязывает только функции, объявленные до `#library`, и хочет все свои: у `SceneObjects` — четыре (03, 6.25).
- Скрипт компилируется, а при запуске `terminate ... stack 'read' error` → имя функции совпало с ключевым словом без учёта регистра (`GoTo` = `goto`). Так же `target` рядом с объектом `Target` — `Duplicate variable name` (03).
- `RUNTIME ERROR ... Not object` на `SendMessage(&Sound, ...)` → локальная переменная с тем же именем без учёта регистра (`string sound`) молча перекрыла глобальный объект `Sound`: внутри функции `&Sound` — уже строка. Ошибки компиляции нет. Переименовать локальную (`alias`).
- `COMPILE ERROR ... missing '('` и `Invalid Expression` на каждом упоминании константы `#define BOARD_ENERGY` → в файле есть функция `Board_Energy(...)`: имена без учёта регистра, и константа читается как вызов функции. Ошибки идут на строках с константой, а не на функции. Давать функциям имена, не совпадающие с `#define` (`Board_FramesCost`).
- В редакторе на Linux не видно курсора, щелчки уходят в старую точку → движок до фикса держал мышь в relative-режиме SDL и в редакторе; теперь `SDLWindow::ShowCursor` его снимает (08, строка 37).
- Горячая клавиша окна ImGui не срабатывает после щелчка по его списку → фокус (`NavWindow`) получил дочерний `BeginChild` с именем `Scene/##objects…`; сравнивать надо `NavWindow->RootWindow->Name` (`scene_tool.cpp`, `Shortcuts`).
- Окно Scene при старте с `--editor` показывает `*` и объекты, изменённые скриптом → скрипты отработали до восстановления Edit. Редактор берёт сцену из `Scene::loaded` и при старте в Edit вызывает те же колбэки, что Stop (05).
- После изменения размера окна кадр рисуется не весь или сцена темнеет → `d3d9->Reset` сбрасывает состояния устройства, а у большинства сущностей обработка `lost_render`/`restore_render` закомментирована. `ResizeBackBuffer` обновляет `OriginalViewPort` (иначе экранная цель рисуется в старом размере), Weather на `restore_render` заново ставит туман, ambient и солнце (`SetCommonStates`). Перед Reset должны быть освобождены все ресурсы `D3DPOOL_DEFAULT` (цели постобработки, текстуры ImGui и кадр Viewport), иначе Reset не проходит. Новая сущность со своими состояниями рендера должна восстанавливать их в `restore_render`.
- Resize окна в middle run: back buffer не меняется, `system.log` has no line `Render: back buffer` → движок always makes the device with `Windowed = TRUE` (`s_device.cpp:InitDevice`), a first `d3d9->Reset` с новым `BackBufferWidth/Height` подает `D3DERR_INVALIDCALL` (`0x887E7804`) с вся variation parameters (SWAP copy, `PRESENTFLAG_INTERVAL_ONE`, `PRESENTFLAG_LOCKABLE`), после а `TestCooperativeLevel` подает `D3DERR_DEVICENOTRESET` (`0x887E7801`) и втора `Reset` с те же parametres подает `SUCCEEDED` (hr 0). `DX9RENDER::ResetDevice` (`s_device.cpp:ResetDevice`) делает имеменно one retry (`ResizeBackBuffer` при не succeeds restore old params). Без retry mid-run resize fails: `xinterface.cpp:Realize` не пере-изнает the screens (its condition compares the size of backbuffer with `dwScreenWidth/Height`, and only `xinterface.cpp:LoadIni` sets them), шрифт, cursor и panel stay в the old size.
- Resize окна в middle run: right and lower edges of the window go beyond the desktop → SDL changes only the size and keeps the left upper corner of the window (`SDLWindow::Resize`). Now the size is clamped to the bounds of the display the window stands on (`SDL_GetDisplayBounds`) and the window is put to the center of that display (`SDL_WINDOWPOS_CENTERED_DISPLAY`), the same way the constructor of `SDLWindow` makes it centered.
- Resize окна в middle run: the app died `0xC0000005` (exit `-1073741819`) в code of the D3D module, `crash_stack.txt` shows `exception_addr` inside `d3d9.dll` → the effect parameter handles cached before the Reset не valid any more. `DX9RENDER::RestoreRender` release все effect objects and compile them again (`s_device.cpp:RecompileEffects`, `effects.cpp:Effects::release` does `fx->Release()` and `effects_.clear()`), а `GetEffectPointer` (`s_device.cpp:GetEffectPointer`) поает a NEW `ID3DXEffect`. Any `D3DXHANDLE` resolved once and kept (`static` in a draw function, a file-level variable) after the Reset point to a dead object, and `eff->SetMatrix(h, ...)` faults inside the D3D module. Резolve handles after the device restored in the `restore_render` handler of the owner: `Grass::RestoreRender` (`grass.cpp:726`) calls `CreateVertexDeclaration` again (1047–1058), which re-takes `fx_` and re-resolves `hgVP_`.
- Entity `modelr` в geometrie не loaded (`SHIP::Mount` of a missing `.gm`, `system.log`: `resource\models\ships\pink\pink.gm: can't open geometry file`) keeps `root == nullptr` (`model.cpp:MSG_MODEL_LOAD_GEO`), а mid-run resize makes the device lost and its `lost_render` handler `MODELR::LostRender` (`model.cpp`) calls `root->ReleaseGeometry()` → the call with `this == nullptr` faults on the first member read в `NODER::ReleaseGeometry`. `MODELR::LostRender` and `MODELR::RestoreRender` guard `root` with `if (root)`, the same guard as `MSG_MODEL_RELEASE` and `MSG_MODEL_RESTORE` make.
- Resize окна в middle run: после пере-изнает the screen the world draws wrong — the model near the camera fills the screen, everything further is cut away and only the sky-blue of the clear color remains → `d3d9->Reset` puts the transforms WORLD, VIEW and PROJECTION back to identity, а `DX9RENDER::RestoreRender` restores самplers, stage states and lights (`SetCommonStates`), but not the transforms. VIEW the camera of the scene sets every frame (`s_device.cpp:SetCamera`), WORLD every draw of a module, but the projection only at the camera init once (`location_camera.cpp:104` `LocationCamera::Init`), or by a script message (`location_camera.cpp:414` `MSG_CAMERA_SET_PERSPECTIVE`), or at a dynamic fog event (`location_camera.cpp:669` `ProcessDynamicFov`) — nothing sets it at an ordinary frame, and `GetTransform(D3DTS_PROJECTION)` reads identity after the Reset (the probes `spdlog::info` before and after `RestoreRender` in `s_device.cpp:ResetDevice` printed `1.3361179 2.3764803 1.000025 1` before and `1 1 1 0` after). With an identity projection the coordinates of the world are the coordinates of clip: the objects of one unit around the camera fill the screen, everything past it is clipped away. `DX9RENDER::RestoreRender` (`s_device.cpp`) makes the projection again for the new screen size with `SetPerspectiveMatrix(Fov, -1.0f)`; `SetPerspective` must not be called again — it multiplies the angle by `FovMultiplier` and `Fov` already keeps it multiplied (`s_device.cpp:2213 Fov = perspective`), the second call would multiply it twice. With `-1.0f` the ratio of the sides comes from the new `screen_size` (`s_device.cpp:2194`), not from the old window.
- `pkill -x engine` не останавливает движок → `build/Debug/engine` — ссылка на `engine-1`, процесс называется `engine-1`.
- `process event stack error` в `error.log` при `ExitProgram` — причина не разобрана, на работу не влияет (08-linux-build.md, таблица фиксов).
- **Вьюпорт сыра в цвот цвета `Render.BackColor`, хотя `system.log` рапорит модель геложден (triangles) и нет ошибким рендера.** Сеbab в custom `.fx` итирда орdr matrix multil reversed — the order of matrix multiplication in the custom `.fx` is wrong: — the engine is row-major, row-vector. Proof in code: `matrix.h:475 CMatrix::EqMultiply(m1, m2)` multiplies as standard m1·m2 without transposition, `matrix.h:458 CMatrix::operator*(const CMatrix&)`, and `matrix.h:465-471 CMatrix::operator*(const CVECTOR&)` computes `matrix[0]*x + matrix[4]*y + matrix[8]*z + matrix[12]`, that is vector × matrix. Matrices pass to the shader raw — `grass.cpp:611 fx_->SetMatrix(hgVP_, cmtx)` with `grass.cpp:568 cmtx.EqMultiply(view, prj)`, `wdm_clouds.cpp:390 fx_->SetMatrix(projectionMatrix_, prj)`. The ready shaders in the repo confirm this: `grass.fx:43 mul(tmpPos, gVP)`, `worldmap.fx:16 mul(vPosition, projectionMatrix)`; in the whole tree `src/techniques` there is not a single case of matrix-first. At the reverse order `mul(gWVP, pos)` every vertex drops into a point (an effect global is initially zero) and the geometry falls out of the screen. See the case `demos/wind-vegetation/RENDER_NOTES.md`. Search: `grep -n 'mul([A-Za-z_]+,' src/techniques`.

## 7.1 Запись видео демо

```
demos/common/record.sh <demo dir> <engine> <out.mp4> <шаг>...
  sleep:S       ждать S секунд
  key:K         нажать и отпустить клавишу (имена xdotool: 1, Escape, space, w, Down)
  hold:K:S      держать клавишу S секунд
  move:DX:DY:S  плавно сдвинуть мышь за S секунд
  type:TEXT     напечатать текст
  click[:X:Y]   щелчок левой кнопкой (с X:Y — сначала перенести мышь в точку окна)
  mousedown:B   нажать кнопку мыши B (1 левая, 3 правая) и держать до mouseup:B
RECORD_AUDIO=1 — записать и звук движка
```
Скрипт запускает `run.sh` демо, ищет окно движка, пишет его ffmpeg-ом (x11grab, libx264, 30 fps), выполняет шаги и останавливает движок. SIGTERM движок перехватывает (SDL превращает его в событие выхода, которое скрипты не обрабатывают), поэтому через 2 с он добивается KILL. С `RECORD_AUDIO=1` движок играет в отдельный null-sink PulseAudio (`PULSE_SINK=storm_record`), ffmpeg пишет его монитор (AAC), а `module-loopback` выводит тот же звук на выход по умолчанию — посторонние звуки системы в запись не попадают, а прогон слышно. После записи оба модуля выгружаются. У обоих входов ffmpeg `-thread_queue_size 1024`, у pulse `-fragment_size 3840` (20 мс): с параметрами по умолчанию ffmpeg ждёт секундные порции звука и между ними берёт один кадр окна, игра идёт с 60+ FPS, а видео со звуком замирает на секунду (так были записаны battle, hud, ai-ship, island, sound и срезы игры; после фикса они перезаписаны, в них меняется 80–100% кадров). Проверка — доля кадров, отличных от предыдущего: `ffmpeg -i v.mp4 -vf scale=160:120,format=gray -f rawvideo -` и сравнение соседних кадров; `mpdecimate` с порогами по умолчанию для этого не годится, он склеивает кадры с мелким движением. Каждая запись добавляет строку в `index.tsv` рядом с видео: дата, демо, коммит движка, файл. С `PREVIEW=S` рядом с видео пишется кадр на S-й секунде (`.jpg`); его берут из готового файла, поэтому ffmpeg останавливается раньше (до фикса `demos/ai-ship` кадр брался из незакрытого mp4 без индекса moov и не получался). Чистовой дубль каждого демо лежит в git: `demos/<демо>/media/<демо>.mp4` и `.jpg` (0,1–5 МБ, без LFS), галерея — в `demos/README.md`; при перезаписи файл заменяется. Запись длится десятки секунд, поэтому из агента её запускают в фоне.

Мышь в окне движка на Linux: игра держит относительный режим мыши SDL (`sdl_input.cpp`), курсор игры начинается там, где был указатель X, а абсолютный `mousemove` до неё не доходит. Поэтому `record.sh` после старта ставит указатель в центр окна, и `move:DX:DY:S` от центра каждый раз приходит в одну точку. В редакторе (Edit, окна) режим снят (`SDLWindow::ShowCursor`), там работает и `click:X:Y` по координатам окна; в Play во вьюпорте и при полёте камеры — снова только `move` и `click` без координат. Свободная камера редактора летает, пока зажата правая кнопка: `mousedown:3 hold:w:1.2 mouseup:3`. После неё курсор ImGui уже не там, где был, поэтому щелчки по окнам ставят до полёта (так записан `demos/scene/media/scene-sound.mp4`).

Пример (демо моря):
```
demos/common/record.sh demos/sea build/Debug/engine videos/sea.mp4 \
    sleep:3 move:300:0:4 hold:w:3 key:2 sleep:5 key:3 sleep:6
```
Кадр для проверки: `ffmpeg -ss 12 -i videos/sea.mp4 -frames:v 1 frame.png`. Шаги отстают от плана на время загрузки сцены, поэтому моменты событий лучше сверять с метками времени Trace в `compile.log`. Движок убивается сигналом, поэтому последние секунды Trace в `compile.log` могут не дойти до диска: в своих прогонах лучше выходить из демо через Esc (`ExitProgram`).

Без нажатий клавиш (окно не забирает фокус, рядом можно работать): сценарий на `PostEvent` сам ведёт игру, а ffmpeg пишет окно по `-window_id`. Звук идёт в null-sink без loopback. Персонаж в локации доходит до точки сам, как NPC: `SendMessage(&chr, "lsfff", MSG_NPCHARACTER_SETTASK, "Runto point", x, y, z)` (30400; ещё есть `"Goto point"`). Путь строится по патчу, а останавливается персонаж примерно в 0,5 м до цели. Корабль игрока ведёт SEA_AI, если снять `MainCharacter`: флаг читается каждый кадр (`ship.cpp`, `AIShipMoveController`). После этого задача `AI_MESSAGE_SHIP_SET_TASK` управляет им и его пушками, как вражеским. Пока окно не отрисовано, x11grab снимает то, что под ним, поэтому начало записи обрезают.

## 7.2 Краш под gdb

Debug-сборка Linux содержит символы, поэтому стек с файлами и строками gdb даёт сразу, без минидампов и разбора RVA:
```
cd <папка игры>
timeout -s INT 90 gdb -q -batch -ex 'handle SIGPIPE nostop noprint' -ex run -ex 'bt 30' build/Debug/engine
```
- При падении `compile.log` и `error.log` остаются пустыми: буфер файлов не сброшен. Сброс из gdb — `-ex 'call (int)fflush(0)'` после `run`. Так видны ошибки компиляции, из-за которых, скорее всего, всё и упало.
- Модель, у которой не нашёлся `.gm` (`can't open geometry file` в `system.log`), остаётся с `root == nullptr`. Методы `MODELR` должны это проверять. Без проверки в `MODELR::Trace` меню, у которого нет моделей пунктов, падало в `NODER::Trace(this=0x0)` из `InterfaceBackScene::CheckMousePos`. Теперь такая модель возвращает 2.0f, то есть «нет попадания».

## 7.3 Подводные камни локаций

- **Камера прыгает при первом переходе.** У `LocationCamera` флаг `isTeleport` стоит с конструктора: первый кадр ставит камеру сразу, без морфинга. Раньше флаг гас только внутри морфинга, поэтому первый `MSG_CAMERA_TOPOS` с плавным переходом (первый диалог в локации) срабатывал как телепорт, а второй уже шёл плавно. Теперь флаг гаснет после первого кадра. Проверка — покадрово (`fps=4,tile=6x3`): у плавного перехода видно 3–4 промежуточных ракурса за ~1 с.
- **Чёрный фон у перьев, серёг, гарды сабли.** Такие части вырезаны альфой текстуры. Техники персонажа (`techniques/animation.fx`) рисовали без альфа-теста, и вырез получался чёрным. Теперь `Animation*` берут альфу из текстуры, `AlphaTestEnable = true`, `AlphaRef = 128` (в `…Blend` — альфа текстуры × `tfactor`, порог 8). У непрозрачных текстур альфа 255, они не меняются.
- **Персонаж застревает у стены по пути.** Таблица путей `.ptc` (`write_ptc` в `demos/common/storm_formats.py`) считает путь в метрах через центры треугольников. На полу из чужих игр вдоль стен лежат цепочки «щепок» шириной 10–20 см, их центры у самой стены. По метрам такой путь короче, и NPC упирается в вершину веера. Поэтому к каждому шагу добавлено `PTC_STEP = 0.5` м: путь идёт по крупным треугольникам. Путь по таблице можно нарисовать, пройдя её от треугольника к треугольнику (2 бита на цель — ребро выхода).
- **Невидимая стена посреди пола.** Персонаж ходит только через общие рёбра треугольников `.ptc`. Если пол рядом есть, но ребро не общее, это стена, которую не видно: герой встаёт посреди улицы и обходит с другой стороны, а Goto к цели за швом принимается, но герой не идёт. Причина — иглы: патчи из чужих игр и пол, нарезанный по контуру «ушами», — это вееры длинных тонких треугольников из одной вершины. После вырезания препятствий клетками куски соседних игл режутся по-разному. Их вершины расходятся на миллиметры или сантиметры, на привязке к сетке веер щепки выворачивается, и складка отрезается как стена. Лечение в генераторе пола (corsairs-remake `tools/pdm_world.py`): ухо выбирается самое «толстое»; препятствия вырезаются выпуклыми кусками без нарезки на треугольники; вершины одного уровня ближе 4 мм сливаются; кусок тоньше 2 мм выбрасывается; у веера остаются только треугольники с ориентацией контура. Найти швы: ребро без соседа, за которым в 3 см снова пол той же высоты (`tools/ptc_seams.py`). На улице Granda Avilia так нашёлся шов 9,7 м вдоль улицы, в таверне вход был отрезан от зала.
- **Вывески и интерьеры теряют прозрачность после моря (Linux).** Модели локаций (`geometry.cpp`) выключают блендинг и держатся на альфа-тесте из общих состояний рендера (`AlphaTestEnable = true`, `AlphaRef = 0xa0`). На Windows каждая техника — D3DX-эффект, он сам сохраняет и возвращает состояния. На Linux `.fx` разбирает `CTechnique`, а у него один набор сохранённых состояний. Если внутри техники рисуют модель со своими техниками, внутренний старт стирает сохранённое внешней, и её состояния остаются навсегда. Так навигатор BATTLE_INTERFACE рисовал остров в `battle_island_gettexture` и оставлял `AlphaTestEnable = false`. Лечится state block D3D вокруг вложенного рисования. Найти утечку: в `TechniqueExecuteStart` писать в лог имя предыдущей техники, если `AlphaTestEnable` уже 0.

## 7.4 Подводные камни контролов и HUD

- **У контрола одна клавиша.** Повторный `MapControl`/`MapKey` того же контрола заменяет клавишу, а не добавляет вторую. Если одно действие нужно на разных клавишах в разных сценах (быстрое действие: F2 на море, пробел на суше), контрол перемапливают при смене сцены.
- **`BI_FastCommand` приходит изнутри `Execute` сущности.** `ILogAndActions` проверяет свой контрол `BIFastCommand` в собственном `Execute` и там же синхронно шлёт событие. Обработчик, который удаляет сущности (переход в другую локацию, выход с моря), уронит движок. Действие откладывают через `PostEvent(..., 0, ...)`.
- **`ILogAndActions` при создании синхронно шлёт `SetWindowSize`.** Обработчик этого события ставят до `CreateEntity`.

## 7.5 Подводные камни моря

- **Форт без `Fort.Mode` молчит.** `AIFort` создаёт пушки, шлёт `FortCreate`, но стреляет, только если у персонажа форта `Fort.Mode` = `FORT_NORMAL` (0); без атрибута `isNormalMode()` ложно, ошибок нет.

## 7.6 Взгляд головы: наклон вбок вместо поворота

Симптом: собеседник в диалоге заваливает голову набок, а не поворачивает её горизонтально. Скриншоты для такой проверки ненадёжны (наклон на одном кадре трудно отличить от позы анимации), поэтому проверка числом.

**Причина.** `CMatrix::RotateX`/`RotateY` (`math/include/matrix.h`) умножают локальную матрицу кости справа на поворот, то есть крутят кость вокруг осей её родителя. Ось Y родителя (шея, позвоночник) в конвертированных моделях не совпадает с мировой вертикалью: родитель стоит под наклоном. Рысканье по его оси Y уводит голову в сторону. Замер на `Billy` (HighRock): крен головы −49° → −23° при `ay = 1.0` рад.

**Что не помогло.** Поворот вокруг сустава в локальной системе кости (`d4b299b2`) и возврат сдвига кости (`5dbde96e`) наклон не убрали: ось поворота оставалась той же.

**Как сделано.** `TurnHead` (`animation/src/animation_imp.cpp`): мировые оси, выраженные в системе родителя, — это столбцы матрицы родителя (матрица ортонормальна, строки — его оси):
- горизонталь (ось тангажа): `(Vx().x, Vy().x, Vz().x)`;
- вертикаль (ось рысканья): `(Vx().y, Vy().y, Vz().y)`.

Голова поворачивается сначала на `ax` вокруг горизонтали, затем на `ay` вокруг вертикали. `RotateAbout` строит поворот в том же соглашении (вектор-строка), что `BuildRotateX/Y`.

**Замер.** В `BuildAnimationMatrices` для головы пишется трасса раз в 120 кадров (только пока взгляд включён):

```
PDM head look: ax=0.000 ay=0.308 roll rest=-79.16 turned=-79.16
```

`roll` — угол оси X головы над горизонталью в модельной системе, в градусах: `asin(Vx().y)`. Трасса идёт через `core.Trace` движка, поэтому попадает в `system.log`, а не в `compile.log` (туда пишут скрипты через `Trace`).

```bash
grep "PDM head look" ~/.local/share/Akella/Sea\ Dogs/Logs/system.log | tail -5
grep "PDM dialog: autotest talked" ~/.local/share/Akella/Sea\ Dogs/Logs/compile.log
```

Критерий: при чистом горизонтальном повороте `turned` равен `rest` с точностью до ~0,1°. Итог после правки: максимальное отклонение 0,01° на 162 замерах при `ay` до −0,31 рад (таверна, GrandaAvilia). До правки — около 26°.

**Что ещё не проверено.** В замерах `ax` оставался 0 (собеседник на той же высоте), поэтому тангаж (наклон вверх/вниз) по числам не подтверждён. Время замеров сопоставляется с диалогом по меткам времени в обоих логах (формат одинаковый), а кадры видео — `ffmpeg -ss` от начала записи.

## 7.7 Моргание глаз гг: сдвиг UV век

Глаза и веки гг нарисованы на одной текстуре `hero_head.tga`, отдельных век в модели нет. Движок «закрывает» глаз, подменяя UV вершин вокруг глаза на строки кожи той же текстуры; новых текстур и анимаций не нужно.

**Как сделано** (`Character::UpdateEyeBlink`, вызывается в конце `Character::Update`):
- Объект головы ищется в `.gm` по текстуре `hero_head.tga` (`FindEyeBlinkObject`). Остальные персонажи с другой текстурой не затрагиваются.
- При смене модели UV объекта копируются в `eyeBlinkUV`; моргание всегда считается от исходных UV, а не от текущих. Вершины доступны через `GEOS::LockObjectVertices` (04 §4.2).
- Окно глаза на текстуре 256×256 (строки сверху вниз): глаз `y = 129..143`, `x = 40..80`, кожа под глазом — строка 145. `closure` идёт 0 → 1 за 0,08 с, держится 0,04 с, 1 → 0 за 0,10 с. Пауза до следующего моргания 1–5 с (первая) и 2,5–5,5 с.
- Строка выборки вершины в строке `y`: `s = mid + (y − mid) / max(1 − closure, 0.001)`, `mid = (129 + 143) / 2`. Если `s` вышла из `[129, 143]`, берётся кожа (145). Открытая часть глаза сжимается к средней линии, а верх и низ закрыты кожей: глаз смыкается от краёв к середине. По `x` сдвиг гасится рампой в 3 текселя (`BlinkRamp`), чтобы не было шва на краю окна.
- Вершины перезаписываются только когда `closure` или сдвиг взгляда изменились.
- **Рывки глаз (саккады).** Каждые 0,4–2,5 с случайно выбирается новая точка взгляда: в 35 % случаев центр, иначе сдвиг до ±3,5 текселя по `u` и ±1,2 по `v`. Глаза переходят в неё за ~0,04 с (быстро, не плавно). Сдвиг получают вершины всего глаза (`u` 36..90, `v` 129..143), поэтому текстура глаза скользит целиком. Оба глаза делят одно окно UV, а сетка зеркальна: знак сдвига по `u` берётся из знака `x` вершины (`eyeBlinkSide`), иначе глаза сходились бы к носу.
- Таймеры моргания и взгляда у каждого персонажа свои (стартовая пауза случайна). `new GEOM` на каждую загрузку модели даёт каждому свой vertex buffer, поэтому одинаковые модели не мигают синхронно.

**Что не вышло.**
- Сдвиг только вниз (кожа поднимается снизу) — весь глаз «ползёт» вверх. Нужна именно симметрия вокруг средней линии.
- Растяжение окна глаза на кожу строки 145 без сжатия сплющивает глаз в полоску. Растяжение зависит от того, на какой строке стоит вершина, поэтому формула считает строку выборки, а не масштабирует UV окна целиком.

**Ограничение.** Вершин в области глаза мало, поэтому закрытый глаз выглядит как тёмный мазок с нерезкими краями; подбираются `kEyeSkin`, `kEyeRamp`, `kEyeMid` (`character.cpp`).

**Замер рывков.** Центр радужки (взвешенный по тёмным пикселям) по кадрам `ffmpeg -vf fps=10,crop=…` в окне одного глаза: вне морганий радужка уходит на 2–3 пикселя от медианы (std ≈ 0,5 px), смещения левого и правого глаза коррелируют (+0,41). Первая версия (сдвиг только внутреннего угла, ±2 текселя) давала меньше пикселя — не видно.

**Проверка.** Автотест `town-face` (06 §6.27): запись 12 с, кадры глаз каждые 0,1 с. Моргание видно на 2–3 кадрах: прикрыт, закрыт, открывается; на кадре «закрыт» глаза нет, брови на месте.

