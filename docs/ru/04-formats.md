# 4. Форматы файлов

| Формат | Где лежит | Кто читает (код) | За что отвечает | Как сделать свой |
|---|---|---|---|---|
| `engine.ini` | корень игры | main.cpp, core_impl.cpp:277, s_device.cpp | конфиг движка | текстом (раздел 2.3) |
| `engine.toml` | рядом с engine | core/src/storm/engine_settings.cpp, config.cpp | переопределение `stash_folder`, `logs_folder` | текстом; Config ищет предпочтительное расширение, затем `.toml`, затем `.ini` |
| `*.c`, `*.h` | `program/` | core/compiler.cpp | логика игры | текстом (раздел 3) |
| `*.fx` | `resource/techniques/**` (исходники: `S/src/techniques/`) | Linux: renderer/technique.cpp (собственный парсер, `#ifndef _WIN32`); Windows: effects.cpp:43 (D3DX) | техники рендера: состояния и шейдеры | `technique Name { pass p0 { AlphaBlendEnable=true; ... PixelShader = compile ps_2_0 f(); } }`; имя техники нечувствительно к регистру |
| `*.tx` | `resource/textures/` (`имя.tga.tx`) | renderer/s_device.cpp:1435 TextureLoad, texture.h | текстуры | 24-байтный заголовок + mip-уровни (4.1) |
| `*.tga` (сырой) | там же | s_device.cpp:1780, **только Windows** | фолбэк через D3DX | на Linux не работает, нужно конвертировать в .tx |
| `*.gm` | `resource/models/` | geometry/src (rdf.h, RDF_VERSION '1.05') | статическая и скиннинговая геометрия, локаторы, кости, BSP | `demos/common/storm_formats.py` `write_gm` (4.2) |
| `*_<свет>.col` | `resource/models/` | geometry.cpp:113 | запечённые цвета вершин (освещение) | генерирует lighter **[не проверено]** |
| `*.ani` | `resource/animation/` | animation | ini: ссылка на скелет и описание действий | текстом (4.3), пример — `make_character.py` |
| `*.an` | `resource/animation/` | animation (an_file.h) | бинарный скелет и ключи | `storm_formats.py` `write_an` (4.3) |
| `*.ptc` | `resource/models/<локация>/` | location (ptc.h, PtcData::Load) | патч персонажей: где можно ходить | `storm_formats.py` `write_ptc` (4.8) |
| `fonts.ini` | `resource/ini/` (`startFontIniFile`) | renderer/font.cpp:69, storm/font_loading.cpp | растровые шрифты | 4.4, из TTF — `storm_formats.write_fonts` |
| `dialog.ini` | `resource/ini/` | dialog/src/dialog.cpp | окно диалога: шрифты, цвета, 9-slice фона, табличка имени, кнопки листания | 4.12, пример — `demos/dialog/make_dialog.py` |
| `*.fnt` (AngelCode, бинарный BMF) | рядом с ini | storm/font_loading.cpp | BmFont (unicode) | BMFont/Hiero, binary, unicode |
| `interfaces.ini` | `resource/ini/interfaces/` | xinterface.cpp:1062 | платформа/масштаб GUI, курсор | 4.5 |
| `defaultnode.ini` | `resource/ini/interfaces/` | xinterface | дефолты для каждого типа узла | 4.5 |
| `<экран>.ini` | `resource/ini/interfaces/` | xinterface (MSG_INTERFACE_INIT 45010) | состав экрана | 4.5 |
| `pictures.ini` | `resource/ini/interfaces/` | xinterface/xservice.cpp:10 | атласы картинок | `[GROUP] sTextureName, wTextureWidth/Height, picture = name,l,t,r,b` |
| `language.ini` | `resource/ini/texts/` | string_service/str_service.cpp | языки, каталоги, шрифты по языку | 4.6 |
| `<lang>/*.txt/*.ini` | `resource/ini/texts/<dir>/` | str_service.cpp | строки `Key,"Text"` | 4.6 |
| `aliases/*.ini` | `resource/ini/aliases/` (без рекурсии) | sound_service | звуковые алиасы | 4.7 |
| `sound_scheme.ini` | `resource/ini/` | sound_service | фоновые схемы | 4.7 |
| wav/mp3/ogg/flac | `resource/sounds/` | sound_service (miniaudio / FMOD) | звук | обычные файлы |
| `*.xps` | `resource/particles/` | particles (particle_manager.cpp:699) | системы частиц | см. 4.11, пишет `storm_formats.write_xps` |
| `particles.ini` | `resource/ini/` и `resource/ini/particles/` | sea_foam, ball_splash, sink_effect, locator/blast | параметры эффектов | по образцу TEHO |
| `TextureSequence.ini` | `resource/ini/` | renderer/xinterface | анимированные текстуры | **[не разбирался]** |
| `star.dat`, `hic.dat`, `hic.txt` | `resource/` | weather/stars | звёздное небо | — |
| `sea%.4d.tga` | `resource/sea/` | sea (sea.cpp) | 64 кадра карты высот волн 128×128 | `demos/sea/make_sea.py` (4.9) |
| `models/ships/<Имя>/*.gm` | `resource/` | ship, mast, rigging (sail.cpp) | корабль: корпус, мачты, реи, паруса как метки | `demos/ship/make_ship.py` (4.10) |
| `resource/foam/locations/` | — | island/foam | пена у берегов | — |
| `models/islands/<имя>/<имя>.gm`, `<имя>_seabed.gm`, `foam/islands/<имя>.{tga,ini}` | `resource/` | island, touch, sea_ai, ship | остров: суша, дно, карта глубин, граф обхода | `demos/island/make_island.py` (4.14) |
| `rigging.ini`, `mast.ini`, `dialog.ini`, `animals.ini`, `loclighter.ini`, `helpchooser.ini`, `interfaces/battle_interface.ini` | `resource/ini/` | одноимённые libs | параметры подсистем | по образцу TEHO |
| видео | `resource/videos/` | xinterface/aviplayer | ролики | на Linux `aviplayer` закрыт `#ifdef _WIN32` (FIX_LINUX) — **видео не работает** |
| Сохранения | путь из скрипта (`save\...`) | core (`SaveEngineState`, compiler.cpp `SaveState`) | состояние всех переменных скрипта (4.13) | `demos/save` |

## 4.1 `.tx` (renderer/include/texture.h)

```
TX_FILE_HEADER (packed, 24 байта):
  int32 flags     ; 1 = TX_FLAGS_PALLETTE (отвергается), 2 = TX_FLAGS_CUBEMAP
  int32 width, height
  int32 nmips
  int32 format    ; TX_FORMAT_*
  int32 mip_size  ; размер mip0 в байтах
затем данные mip-уровней подряд (для cubemap — грани front, right, back, left, top, bottom)
```

| format | значение | Поддерживается (textureFormats, s_device.cpp:346) |
|---|---|---|
| A8R8G8B8 | 21 | да (байты BGRA) |
| X8R8G8B8 | 22 | да |
| R5G6B5 | 23 | да |
| A1R5G5B5 | 25 | да |
| A4R4G4B4 | 26 | да |
| DXT1/DXT3/DXT5 | FourCC `'1TXD'`… | да |
| P8 (41), L8 (50), V8U8 (60), L6V5U5 (61) | — | нет |

Путь: `TextureCreate("interfaces\menu.tga")` → `resource\textures\interfaces\menu.tga.tx`. Префикс и `.tx` добавляются, если их нет (s_device.cpp:1301/1435). `texture_degradation` пропускает первые mip-уровни. Генератор: `tools/menu-demo/make_assets.py` (`struct.pack("<6i", 0, w, h, 1, 21, len)` + BGRA).

## 4.2 `.gm` (geometry/src/rdf.h)

Порядок блоков: `RDF_HEAD` (version '1.05', flags, name_size, ntextures, nmaterials, nlights, nlabels, nobjects, ntriangles, nvrtbuffs, bbox, radius) → таблица имён → textures → materials → lights → labels (локаторы/кости) → objects → triangles → vertex_buffers → vertices → BSP. Сообщения модели: `MSG_MODEL_LOAD_GEO 20500 "ls"`, `LOAD_ANI 20501`, `SET_POSITION 20506`, `SET_DIRPATH 20509`, `SET_TECHNIQUE 20510`, `PLAY_ACTION 20601`.

Что важно при генерации (подробно — в docstring `write_gm`):
- Цвет вершины — альбедо, на которое умножается свет, текстура накладывается с MODULATE2X: серый `0x80` оставляет текстуру как есть.
- D3D отсекает грани против часовой стрелки: треугольник ориентируется так, чтобы `cross(b−a, c−a)` смотрел вдоль нормалей.
- Скиннинговые вершины (`RDF_AVERTEX0`, бит 4 в `RDF_VERTEXBUFF.type`) движок при трансформации отражает по x (`model.cpp`, VBTransform), поэтому их хранят зеркально.
- Метки (`RDF_LABEL`, `<3i16f4i4f>`: группа, имя, flags, матрица 4×4 строками Vx, Vy, Vz, Pos, bones, weights) — `write_gm(..., labels=[(group, name, pos)])`. Группы движок сравнивает по адресу строки (`unbelievable_workaround`), поэтому одинаковые имена в таблице строк должны быть одной записью.
- Метки статической модели локации — её локаторы: `Location::LoadStaticModel` раскладывает их по группам (`goto`, `reload`, `item`…) в `LocatorArray`. Повернуть метку вокруг вертикали — `(group, name, pos, (x, z))`, ось z смотрит вдоль `(x, z)`; пример — `LOCATORS` в `demos/location/make_location.py`.
- Метка, привязанная к кости (оружие персонажа: `saber_hand`, `saber_belt`), — `labels=[(group, name, pos, y_dir, bone)]`: `bones[0] = bone`, вес 1, ось y метки смотрит вдоль `y_dir`. `BLADE` ставит клинок матрицей `scl(−1,1,1) · mt · skin[bone]`, где у Linux-сборки x отражается ещё раз. Поэтому для скиннинговой модели метку хранят сопряжённой зеркалом, M·L·M: элементы 3×3, где ровно один индекс равен 0 (x), меняют знак, и `pos.x` тоже (`write_gm`). Модель оружия грузится из `models\Ammo\<имя>` с текстурами из `textures\Ammo\`. Метки `start`/`end` в ней задают след клинка.
- Без BSP-блока (`FLAGS_BSP_PRESENT`) `GEOM::Trace`/`Clip` не попадают в модель: нет теней на ней, камера проходит сквозь стены, ядра — сквозь корпус. Глубина дерева ≤ 250 (стеки на 256). У выпуклой модели (корпус) плоскость любой грани оставляет остальные по одну сторону, и дерево из одних граней растёт на 15 граней за уровень; поэтому `bsp_tree` в таком случае делит треугольники узлом без граней по медиане вдоль длинной оси (`nfaces = 0` движок пропускает, collide.cpp).

## 4.3 Анимация

`.ani` — это ini: ключ `animation = <файл .an>` плюс секции-действия с ключами `start_time`, `end_time`, `speed`, `type` (normal/reverse/pingpong/rpingpong), `loop` (true/false), `event` (always/normal/reverse), `data`, `bone`.

`.an` (an_file.h): `HEADER{nFrames, nJoints, framesPerSec}` → `parentIndex[nJoints]` → `jointPos[nJoints]` → `rootPos[nFrames]` → `jointAngle[nJoints][nFrames]` (позиции — CVECTOR, повороты — кватернионы x, y, z, w).
- Кадр 0 — bind pose: по нему строятся обратные матрицы, меш должен совпадать с ним.
- Как и меш, всё хранится зеркально по x: позиции `−x`, кватернионы `(x, −y, −z, w)`.

Пример `.ani` (из `demos/location`, генерирует `make_location.py`); персонаж `location`/`player` ищет действия по этим именам:
```ini
animation = sailor.an        ; рядом с .ani
[idle_1]
start_time = 1               ; кадры .an; кадр 0 — bind pose, в действия не входит
end_time = 121
loop = true
[walk]
start_time = 123
end_time = 155
loop = true
data = "Walk speed", "1.312"     ; м/с, персонаж двигается с этой скоростью
[back walk]
start_time = 123
end_time = 155
loop = true
type = reverse
data = "Walk speed", "-1.312"
```
События кадра в действии: `event = "Attack", 50%` (процент длины действия или номер кадра; третьим словом — always/normal/reverse). Персонаж реагирует на `Attack` (момент удара), `Parry start`/`Parry end`, `Feint start/end`, `Recoil start/end`, `Blade to hand`/`Blade to belt` (сабля переходит из ножен в руку в действиях `Normal to fight`/`Fight to normal`; если этих действий нет, сабля переходит сразу), `Death sound`, `LStep`/`RStep`. Без `Attack` удар не наносит урона.
Также нужны `run`, `back run` (скорость 3.6) и `turn left`, `turn right`, иначе персонаж стоит. Скорость в `data` должна совпадать с шагом анимации, иначе ноги скользят.

## 4.4 Шрифты (fonts.ini)

Секция `[имя_шрифта]` поддерживает два варианта:

| Вариант | Признак | Ключи |
|---|---|---|
| BmFont | есть ключ `file` | `file` (бинарный AngelCode .fnt, сигнатура BMF, unicode; страницы — относительно .fnt), `pcscale`, `line_spacing`, `gradient` (текстура). Техники `BmFont` / `BmFontGradient` (`techniques/interfaces/font.fx`) |
| Классический (FONT::Init, font.cpp:69) | нет `file` | `Texture`, `Techniques` (обычно `InterfaceFont` из `interfaces/game_interface.fx`), `Height`, `AspectHeightConstant`, `pcscale`, `Texture_xsize/ysize`, `Symbol_interval`, `Shadow` (сам факт наличия ключа), `Shadow_offsetx/y` (2), `Spacebar` (8), `char_X = x,y,w,h` для кодов 30..0x206F |

Особенности ключей символов: строчная латиница записывается как `char_a_` (ini регистрозависим), знак `=` — как `char_equ`.

Из TTF шрифт делает `storm_formats.write_fonts({имя: (ttf, размер, обводка)}, resource)`: рисует атлас `textures/fonts/<имя>.tga.tx` (латиница, кириллица, знаки) и пишет секции `[ИМЯ]` в `ini/fonts.ini` с техникой `InterfaceFont`.

## 4.5 Интерфейс (xinterface)

**`interfaces.ini`** (xinterface.cpp:1062). Секция `PC_SCREEN` или `PC_SCREEN_W_H` выбирается по соотношению сторон окна. Ключи: `bDynamicScaling`, `fScale`, `wScreenWidth/Height`, `wScreenLeft/Top/Right/Down`. В `[COMMON]`: `MousePointer = tex,sensitivity,size:(w,h),pos:(x,y)`, `mouseDblClickInterval` (300), `RepeatDelay` (500), `BlindTime`, `blendSpeed`, `wave*`. Если ini экрана не найден, движок шлёт `PostEvent("exitCancel")`.

**Экран `<screen>.ini`**:
```ini
[MAIN]
item = [priority,][PC|LANG,lang,]TYPE,NAME   ; приоритет по умолчанию 80
start = B_NEWGAME                           ; узел с фокусом
glow = GLOWCURSOR,GLOWCURSOR                ; по умолчанию

[B_NEWGAME]
position = 300,200,500,240
string = NewGame                            ; ключ строки (TEXTBUTTON)
command = click
command = activate
command = downstep,select:B_EXIT
command = deactivate,event:Menu_Quit
```

Типы узлов (NewNode, xinterface.cpp:1486): BUTTON, TEXTBUTTON, PICTURE, TWO_PICTURE, SLIDEPICTURE, SCROLLEDPICTURE, IMAGECOLLECTION, STRINGCOLLECTION, LINECOLLECTION, RECTANGLE, BOUNDER, FRAME, TITLE, FORMATEDTEXT, QUESTTITLE, QUESTTEXT, CONTEXTHELP, SCROLLBAR, SCROLLER, SCROLLIMAGE, VIMAGESCROLL, CHANGER, LRCHANGER, SLIDER, CHECKBUTTON, EDITBOX, PCEDITBOX, KEYCHOOSER, TABLE, WINDOW, STATUSLINE, FOURIMAGES, GLOWS, GLOWCURSOR, VIDEO, VIDEORECTANGLE.

Общие ключи узла (inode.cpp:480): `position`, `bAbsoluteRectangle`, `bShowGlowCursor`, `bGlowCursorToBack`, `bNotUse`, `bWheelUse`, `HelpTextureFile`, `HelpTextureUV`, `GlowRectangle`, `GlowOffset`, `tooltip`, `UseActionByDeclick`, `bBreakCommand`, `moveMouseDoSelect`. Ключ `command` читается только из экранного ini, не из defaultnode.

Команды (xutil.cpp:12): `activate, deactivate, select, click, dblclick, rclick, dblrclick, upstep, downstep, leftstep, rightstep, speedup, speeddown, speedleft, speedright`. Подпараметры команды: `select:NODE`, `com:cmd:node`, `event:Name` (событие `"ls"`: индекс команды, узел; xinterface.cpp:287), `delay:N`, `sound:N`. Каждая сработавшая команда также шлёт `Event("ievnt_command","ss",cmd,node)` (inode.cpp:142).

**`defaultnode.ini`**: секция на тип узла, ключ `class` подменяет C++-класс. Остальные ключи — дефолты для всех узлов этого типа (например, TEXTBUTTON: `font`, `fontColor`, `faceColor`, `buttonLeft/Middle`, `strOffset`, `pressDelay`, `pressPictureOffset`).

В TEXTBUTTON `string` — ключ в таблице строк. В других узлах строка с `#` в начале — литерал.

Коды сообщений GUI: `MSG_INTERFACE_INIT 45010` (`"ls"`, путь к ini), остальные 45011–45053 — в `shared/interface/messages.h`.

## 4.6 Строки и языки (string_service/str_service.cpp)

`resource\ini\texts\language.ini`:
```ini
[COMMON]
GlobalFile = common.ini     ; таблица строк
defaultLanguage = Russian   ; по умолчанию English
[DIRECTORY]
Russian = russian
[FONTS]
Russian = fonts.ini         ; → resource\ini\fonts.ini (SetFontIniFileName)
```
Формат строк: `string = Key,"Text"` (без `#`). Дополнительные файлы подключаются из скрипта: `id = LanguageOpenFile("x.txt"); LanguageConvertString(id, "Key"); LanguageCloseFile(id)` → `resource\ini\texts\<dir>\x.txt`.

## 4.7 Звук (sound_service, sound_defines.h)

Алиас (`resource\ini\aliases\*.ini`):
```ini
[click]
prior = 128
minDistance = 1
maxDistance = 50
volume = 1.0
name = interface\click1.wav, 1
name = interface\click2.wav, 1   ; несколько name — случайный выбор с весом
```

`SoundPlay` (sound_service.cpp:290): имя без `\` сначала ищется среди алиасов, иначе файл берётся из `resource\sounds\`. Алиас задаёт файл (случайный по весам), `minDistance`/`maxDistance`, `volume`, `prior` — они заменяют переданные в сообщении. Алиасы читаются один раз при старте сервиса.

Фоновая схема (`resource\ini\sound_scheme.ini`), включается `MSG_SOUND_SCHEME_SET`:
```ini
[sea_day]
ch = gull, 3, 9, 0.45        ; звук или алиас, пауза от 3 до 9 с, громкость
ch_loop = demo\waves.wav     ; зациклен с момента установки схемы
```
Звуки схемы — стерео, без позиции. `MSG_SOUND_SCHEME_RESET` только перестаёт запускать новые; `ch_loop` после сброса продолжает играть (id его канала сервис не хранит), поэтому зацикленный фон лучше запускать из скрипта и останавливать по id (`demos/sound`).

Из скрипта звук управляется через сущность `Sound` (sound/src/sound.cpp). Первым в формате идёт `l` — код сообщения:

| Сообщение | Код | Формат после кода |
|---|---|---|
| MSG_SOUND_PLAY | 77003 | `s` файл или алиас, `l` тип, `l` канал громкости, `l` только закэшировать, `l` looped, `l` cached, `l` fade in, мс; дальше для 3D: `fff` позиция, `ff` min/max дистанция; для стерео-стрима с looped: `l` пауза между повторами, `f` громкость. Хвост можно опускать. Возвращает id |
| MSG_SOUND_STOP | 77004 | `ll` id (0 — все), fade out, мс |
| MSG_SOUND_RELEASE | 77005 | `l` id |
| MSG_SOUND_SET_3D_PARAM | 77007 | `ll` id, параметр (`SOUND_PARAM_POSITION 3` + `fff`; `MAX_DISTANCE 1`/`MIN_DISTANCE 2` + `f`) |
| MSG_SOUND_SET_VOLUME | 77008 | `lf` id, громкость |
| MSG_SOUND_IS_PLAYING | 77009 | `l` id |
| MSG_SOUND_SCHEME_RESET | 77013 | — |
| MSG_SOUND_SCHEME_SET | 77014 | `s` секция `sound_scheme.ini` |
| MSG_SOUND_ALIAS_ADD | 77017 | `s` файл алиасов |
| MSG_SOUND_SET_MASTER_VOLUME | 77021 | `fff` эффекты, музыка, речь (0..1) |

Типы (`eSoundType`): `PCM_3D 1`, `MP3_STEREO 2` (музыка: стрим с диска), `MP3_3D 3`, `PCM_STEREO 4`. Каналы громкости: `VOLUME_FX 1`, `VOLUME_MUSIC 2`, `VOLUME_SPEECH 3`. Форматы файлов — wav/ogg/mp3/flac (miniaudio); бэкенд (miniaudio/FMOD) — см. [08-linux-build.md](08-linux-build.md) §7.

- **Слушатель** — камера: сервис каждый кадр берёт позицию и ориентацию из матрицы вида рендера (`RunStart`), из скрипта ничего делать не нужно.
- **3D**: громкость 1 до `min`, линейно до 0 к `max`; дальше `MAX_DISTANCE` (500 м) звук не создаётся.
- **Музыка** играет в двух слотах (id 1 и 2): новый трек наплывает за fade in, прошлый за то же время затихает. Позиция остановленного трека запоминается по имени, при повторном запуске он продолжает с неё.
- **Fade in/out** у обычных звуков (не музыки) раньше не работал: фейдер обрабатывался только для двух слотов музыки, звук с fade in оставался беззвучным, с fade out не останавливался. Исправлено вместе с `demos/sound` (`SoundService::RunStart`, `SoundPlay`).

## 4.8 Патч персонажей `.ptc` (location/include/ptc.h)

Персонажи ходят только по треугольникам патча (подробно — docstring `write_ptc`):
- Ребро, общее у двух треугольников по обеим вершинам, — проход, остальные — стены (персонаж скользит вдоль). Ступенька без общих вершин — тоже стена.
- `PtcData::FindNode` считает точку внутри, если она слева от каждого ребра при взгляде сверху: обход с `cross(b−a, c−a).y > 0`.
- `nb[j]` — сосед через ребро `i[j] → i[j+1]`, `−1` — стена.
- Сетка поиска: ячейки ~1 м, в каждой список треугольников.
- Таблица путей: строка на треугольник, по 2 бита на цель — через какое ребро выходить (BFS), 3 — недостижим.

## 4.9 Кадры волн моря `sea%.4d.tga` (sea/src/sea.cpp)

`resource\sea\sea0000.tga … sea0063.tga` — 64 кадра 128×128, несжатый TGA 32 бита, высота в первом (синем) байте пикселя. Кадры проигрываются по кругу и тайлятся по морю, поэтому волны должны быть периодичны и в пространстве, и во времени (в `make_sea.py`: целые волновые числа и целое число периодов за цикл). Цвет, амплитуды, масштабы и пена задаются атрибутами `Sea.Sea2.*` (см. рецепт 6.9).

## 4.10 Модель корабля (ship/src/ship.cpp, geometry/src/node.cpp, rigging/src/sail.cpp)

`resource\models\ships\<Имя>\<Имя>.gm`, имя — `BaseShip.Name`. Метка в группе `geometry` подгружает дочернюю модель `<Имя>_<метка>.gm` (node.cpp:226) и вешает её в точку метки: так собирается дерево корпус → `mastN` → `rey_*`.
- Узел `mastN` задаёт номер мачты, узел `rey*` с родителем `mastN` — рею на ней; `rey_` — рея поворачивается по ветру; 5-й символ `a` — быстрый парус (FAST).
- Паруса — метки в группах `sail<тип><N>` (`AddSailLabel`). Тип: `s`, `g`, `d` — прямоугольный; `t`, `v` — треугольный; `f`, `n` — трапеция; `x` — особый. Имена меток — углы: `sail1`→hp0, `sail2`→hp1, `sail3`→hp3 (у треугольного hp2), `sail4`→hp2, `sail0` — limit point.
- Прямоугольный: hp0, hp1 — верхние углы, hp2, hp3 — нижние (ширина = |h0+h2−h1−h3|/2). Треугольный: ширина h2−h0, высота h1−h2.
- Текстуры: ткань — что вернёт `GetSailTextureData` (`normalTex`), плюс `ships\parus_hole.tga` (дыры; альфа умножается, поэтому непрозрачная) и `ships\emptygerald.tga` (герб; прозрачный).
- `resource\ini\rigging.ini` [SAILS] обязателен. Значения разбирает `parseValue`; цвета — целое 0xRRGGBBAA, записанное десятичным числом (`ColorToD3D`: r — старший байт), например `Diffuse = 2576980479`.

## 4.10.1 Ядра `AIBalls` (sea_ai/src/ai_balls.cpp)

Атрибуты сущности, читаются по `isDone` (один раз): `Texture` (атлас спрайтов, `textures\<путь>.tx`), `SubTexX`/`SubTexY` (размер сетки атласа), `SpeedMultiply` (общий множитель времени), `BallFlySoundDistance` и `BallFlySoundStereoMultiplyer` (ядро ближе этого к камере шлёт `Bll_FlyNCam` "fff"), `Balls.<тип>.{SubTexIndex, GoodIndex, Size, Weight, Particle}` — типы ядер, `<тип>` — имя.

Выстрел — присвоить корневые атрибуты и `add`: `Type` (имя типа), `CharacterIndex` (чьё ядро: свой корабль его пропускает), `x`/`y`/`z` (дуло), `SpdV0` (м/с), `Ang` (возвышение, рад), `Dir` (курс, как `ship.ang.y`: x = sin, z = cos), `HeightMultiply`, `SizeMultiply`, `TimeSpeedMultiply`, `CannonType`, `MaxFireDistance`, `RawAng`, `Event` (пустая строка — ядро рисуется; иначе событие каждый кадр и спрайта нет). `clear` удаляет все ядра. Полёт — парабола с g = 9.81, трассируется по отрезку за кадр: паруса (`sail`), корабли в слое `SHIP_CANNON_TRACE 23`, форт, остров, море. Попадание в море шлёт `Ball_WaterHit` "lfff" (владелец, точка).

Попадание в корабль (ship.cpp `Cannon_Trace`): мачта — `ShpMastDamage` "llffffal" (тип касания: 2 — ядро, номер мачты, точка, урон 0..1, aref персонажа, владелец), обработчик **обязан** вернуть float — новый урон, при ≥ 1 мачта падает и приходит `EventMastFall` "lsl"; корпус — `Shp_HullHit` "illffflf" (сущность, владелец, индекс корабля, точка, ближайшее место пожара и расстояние до него) и пробоина `MSG_BLOTS_HIT` (сущность `blots`, текстура `resource\textures\blot.tga.tx`: атлас 2×2); парус — дыра и `ShpSailDamage` "lfffl".

## 4.10.2 Пушки корабля (sea_ai/src/ai_ship_cannon_controller.cpp, ai_cannon.cpp)

Борта — классы `BaseShip.Cannons.Borts.<борт>` типа корабля (`AI_MESSAGE_ADD_SHIP` передаёт его третьим аргументом): `FireDir` — направление борта от носа по часовой (рад; левый `PI*1.5`, правый `PI*0.5`), `FireZone` — сектор, который борт накрывает, `FireAngMin`/`FireAngMax` — пределы возвышения. Пушки борта — метки модели корабля в группе с тем же именем, что и борт (`cannonl`, `cannonr`; сравнение без учёта регистра). Положение метки — дуло, ось z её матрицы — направление ствола. Ствол, повёрнутый от цели больше чем на ~72° (скалярное произведение < 0.3), не стреляет. В `write_gm` метка с направлением — кортеж `(группа, имя, позиция, (x, z))` (4.2). У персонажа `Ship.Cannons.Borts` должен существовать: SEA_AI пишет туда по классу на борт с зарядом и дальностью борта.

## 4.11 Системы частиц `.xps` (particles/src/system/data_source)

`resource\particles\<имя>.xps`, все файлы грузятся при старте сервиса частиц, система создаётся по имени файла. Little-endian:
- Заголовок: `"PSYS"`, `"v3.5"`, uint32 число эмиттеров. Эмиттер: uint32 тип (0 — POINT), список полей, uint32 число частиц; частица: uint32 тип (1 — BILLBOARD, 2 — MODEL), список полей.
- Список полей: uint32 count, затем поля: uint32 тип, данные, имя (uint32 длина+1, байты, `\0`; длина < 128). Типы и данные: BOOL 1 — uint32; FLOAT 2 — float; GRAPH 3 — uint32 negative, uint32 relative, граф max (uint32 count, пары float время/значение), граф min; POSITION 4 — 3 float; STRING 5 — 128 байт; UV 6 — count, по 4 float x, y, w, h (движок прибавляет x, y к w, h); COLOR 7 — count, на точку: время, цвет max (4 float rgba 0..1), цвет min.
- Отсутствующие поля создаются с дефолтами (в основном 0), поля чужого типа удаляются. Значение — случайное между min и max.
- Графы читаются по времени эмиттера в секундах (последняя точка — до 99999, после неё 0), с `relative` — по возрасту частицы в процентах 0..100; цвет всегда по возрасту 0..1. Отрицательные значения обнуляются без `negative`.
- Эмиттер: `Name`, `Life time` (с), `Looping`, `Position`, `Physics Dir X/Y/Z` — разброс направления в градусах от +y.
- Billboard: `Emission rate` (в секунду), `Max Particles count`, `Life time`, `Velocity`, `Mass` (отрицательная — всплывает), `ForceField_gravity` (% от 9.8·масса), `Friction` (%), `Spin` (град/с), `Size`, `Color`, `Transparency` (0 — непрозрачно, 100 — невидимо), `Texture` (UV кадров атласа), `Animation frame` (индекс кадра), `Add Technique Power` (100 — аддитивно, 0 — обычное смешивание), `Track X/Y/Z` (при времени 0 — смещение рождения), `Attached emitter`.
- Рисуются техникой `AdvancedParticles` с атласом `resource\textures\particles_list.tga` и нормал-картой `particles_listnm.tga`; rgb умножается на (dot(нормаль, …) + 0.6), поэтому плоская нормал-карта (128, 128, 255) — ровный свет. Низ billboard берёт верх кадра (TY1).

## 4.12 Окно диалога `dialog.ini` (dialog/src/dialog.cpp)

Читается при создании сущности `dialog`. Координаты — в экране `baseScreenSize` (обычно 800,600), движок масштабирует их под окно.
- `[DIALOG]`: `mainfont` (реплика), `subfont` (ответы), `charnamefont` (имя на табличке) — имена секций fonts.ini; `mainFontScale`, `subFontScale`, `charnamescale`; цвета десятичным ARGB: `mainFontColor`, `subFontColor` (невыбранный ответ), `subFontColorSelect`, `charnamecolor`; `maxtextlines`, `maxlinkslines` — строк на страницу; `charnameoffset` — текст имени относительно таблички.
- `[BACKPARAM]`: `texture` — атлас окна; `uvLeftTop`, `uvTop`, `uvRightTop`, `uvLeft`, `uvCenter`, `uvRight`, `uvLeftBottom`, `uvBottom`, `uvRightBottom` — 9-slice фона (UV 0..1), `backBorderOffset` — толщина рамки; `backPosition` = left,top,right,bottom (верх пересчитывается по высоте текста, окно растёт вверх от `bottom`); `uvDivider`, `dividerHeight`, `dividerOffsetX` — линия между репликой и ответами; табличка имени — `uvChrNameLeft/Center/Right`, `ChrNameLeftWidth`, `ChrNameCenterWidth` (заменяется длиной имени), `ChrNameRightWidth`, `ChrNameHeight`, `chrNameOffset`.
- `[BUTTON]`: стрелки листания — `uvUpNormal`, `uvUpLight`, `uvDownNormal`, `uvDownLight`, `buttonSize`, `rightoffset`, `topoffset`, `bottomoffset`.
- Текст рисуется в области фона минус рамка, с отступом 20 по горизонтали; перенос — по словам (`dialog_utils.cpp` `AddToStringArrayLimitedByWidth`).
- Щелчок при выборе — `resource\sounds\interface\ok.wav`.

## 4.13 Файл сохранения (core/src/compiler.cpp `SaveState`/`LoadState`)

Пишет `SaveEngineState(path)`, путь — как у любого файла движка (на Linux — от cwd, каталог должен существовать до старта, 2.2).
- `EXTDATA_HEADER` (compiler.h): `char sFileInfo[32]` — строка из глобальной переменной скрипта `savefile_info` (иначе `"save"`), `uint32 dwExtDataOffset`, `uint32 dwExtDataSize` — блок доп. данных в конце файла (его дописывает `SetSaveData`, так xinterface хранит описание и скриншот для экрана загрузки).
- `uint32` длина распакованных данных, `uint32` длина сжатых, данные zlib (`compress2`, максимальное сжатие).
- Внутри: каталог программы, таблица строк кодека атрибутов (`SCodec`), имена загруженных сегментов (сохраняется только первый — файл из `run` в engine.ini), затем все глобальные переменные: имя + значение, у `object` — со всем деревом атрибутов.
- Сущности, обработчики событий, слои и отложенные события не сохраняются. Загрузка ищет переменные по имени, так что файл переживает перестановку кода, но не переименование переменных.

## 4.14 Остров (island/src/island.cpp, ai_flow_graph.h)

`SendMessage(&Island, "lsss", MSG_ISLAND_LOAD_GEO 50100, "islands", <каталог моделей>, <имя>)` грузит `resource\models\<каталог>\<имя>.gm`, а затем карту глубин `resource\foam\islands\<имя>.tga` и её `<имя>.ini`. Первый аргумент — каталог карты внутри `foam`.
- **Модель.** Метка группы `geometry` с именем `seabed` подгружает `<имя>_seabed.gm` дочерним узлом (как мачты, 4.10). `Mount` отцепляет узел `seabed` в отдельную модель. Остров рисует её сам: после суши, без тумана и не в отражении моря. Обе модели — в слое `ISLAND_TRACE`: по ним трассируют ядра, камера и SEA_AI. Поэтому у острова нужен BSP (4.2).
- **Бокс карты.** Это объединение боксов всех узлов в `ISLAND_TRACE` (суша и дно) плюс 50 м по x и z (`CalcBoxParameters`). Карта квадратная, N×N, N — степень двойки. Шаг — размер бокса / N по каждой оси отдельно. Пиксель `(i, j)` — точка `x = center.x + (i − N/2)·step`, `z = center.z + (j − N/2)·step`, строка — это z.
- **`<имя>.tga`.** Заголовок TGA 18 байт (`type 3`, `bpp 8`, `attr8 8`), затем N² байт без сжатия. Байт `v` — высота дна `−20·(v − 2)/253` м: 2 — уровень моря и суша, 255 — 20 м и глубже, 0 тоже −20. Вне бокса `GetDepth` возвращает false и −50.
- **`<имя>.tga.zap`.** При первой загрузке движок упаковывает карту в этот файл и дальше читает только его. Новая карта со старым zap не подхватится: генератор удаляет zap.
- **`<имя>.ini`.** Без него срабатывает `Assert`.
  - `[Main] vBoxCenter = x,y,z`, `vBoxSize = x,y,z` — центр и **половина** бокса из пункта выше. При расхождении больше 0.1 движок пишет в `system.log` «vBoxCenter not equal, foam error», но работает.
  - `[GraphPoints] pnt<i> = x,z,N,a0,b0,a1,b1,…` — граф обхода (`AIFlowGraph`). Точка, затем число рёбер и пары индексов их концов. Пустая секция допустима. SEA_AI берёт точку графа, если прямая до цели пересекает модель острова (`GetMovePoint` трассирует на y = 0.1). Точки ставят на чистой воде вокруг острова, глубже −14 и дальше 80 м от берега.
- **Без карты** (`WARN: FOAM: Can't find foam`) движок строит её сам — трассировкой модели 2048×2048. Это долго, и граф обхода остаётся пустым.
- **Генерация** (`demos/island/make_island.py`). Рельеф задаётся одной функцией `height(x, z)`. Из неё строятся сетка суши (только ячейки выше −8, с BSP), сетка дна (обрезана сверху на −8.5, чтобы не пробивать сушу), карта глубин и проверка точек графа. Модель, карта и граф поэтому не расходятся.

## 4.15 Глобальная карта (worldmap/src)

Сущность `worldmap` грузит всё по фиксированным именам, от `resource\models\WorldMap` и `resource\textures\WorldMap`. Генератор — `demos/worldmap/make_worldmap.py`.
- **`islands\islands.gm`** — базовая модель. Её не рисуют, читаются только метки:
  - группа `locators`: `min`/`max` — углы мира по x/z;
  - `islands`: точка каждого острова, имя метки = имя модели острова;
  - `labels`: точки надписей с названиями (скрипт привязывает к ним текст, 6.20);
  - `merchant`: порты, между которыми ходят торговцы.
- **`islands\<имя>.gm`** — суша. Корабль упирается в то, что выше y = 0.5 внутри бокса модели (`GEOM::Clip`), поэтому нужен BSP (4.2). Береговую линию резать по h = 0 треугольниками, а не целыми ячейками сетки: иначе берег зубчатый.
- **`islands\<имя>_area.gm`** — плоский диск вокруг острова **в мировых координатах** (не относительно острова), тоже с BSP. Корабль внутри диска пишет имя острова в `worldMap.island`.
- **`Ship.gm`, `WindRose.gm`** — корабль игрока и компас. Корабли встреч грузятся по именам, которые шлёт скрипт (`merchant`, `pirate`).
- Модели рисуются без света (`WdmModelDrawStd` берёт текстуру как есть), поэтому освещение запекают в текстуру.
- **`Sea\sea.tga`** — море на весь квадрат мира: u вдоль x, v от +z вниз. Поверх него — 32 кадра ряби `Sea\Ani\wave_0000..0031.tga` и блики `Sea\flare.tga`.
- **`clouds.tga`** — атлас 2×2 из четырёх туч. `cloudslight.tga` и `stormlight.tga` — градиенты освещения обычных и грозовых туч. Ещё `lightnings.tga`, `rain.tga`, `csmoke.tga` (дым выстрелов) и `watermark.tga` (кильватер).
- **`Interfaces\*`**:
  - часы: `sky.tga` — полоса неба на сутки, `sky_mask.tga`;
  - ветер: `wind_pointer`, `bar`, `bar_mask`;
  - мораль и провиант: `morale`, `morale_bar`, `morale_mask`;
  - `back`, `coord`;
  - флаги наций `WorldMapEnsigns`, иконки встреч `BigIcons`.
- **Шрифты** берутся из атрибутов `worldMap.date.font` и `labels.*.font` (`fonts.ini`, 4.4).

## 4.16 Сцена `.scene` (scene/src/storm/scene/scene.cpp)

Расстановка моделей, которую сохраняет окно Scene редактора (6.21) и грузит скрипт через `LoadScene` (3.5). Лежит в `resource/scenes/<имя>.scene`, формат — TOML: массив таблиц `[[model]]` в порядке создания, за ним `[[marker]]` и `[[sound]]`.

| Ключ | Тип | Что |
|---|---|---|
| `name` | строка | имя в окне Scene, уникальное в сцене |
| `geometry` | строка | модель как для `MSG_MODEL_LOAD_GEO`: путь от `resource/models` без `.gm`, через `\` (`'demo\crate'`) |
| `technique` | строка, необязательно | `MSG_MODEL_SET_TECHNIQUE` (`DLightModel` — свет солнца); нет — своя техника модели |
| `textures` | строка, необязательно | папка `resource/textures`, в которой лежат текстуры модели, если в `.gm` они названы без папки (`'characters'`: `sailor_face.tga` → `textures/characters/sailor_face.tga.tx`). На время загрузки это `SetTexturePath`, как у персонажа |
| `animation` | строка, необязательно | скелет для `MSG_MODEL_LOAD_ANI`: `resource/animation/<имя>.ani` (`'fencer'`) |
| `action` | строка, необязательно | действие, которое проигрыватель 0 играет с загрузки (`'idle_1'`); зацикливается, если в `.ani` `loop = true` |
| `position` | 3 числа, необязательно | место в метрах; нет — 0, 0, 0 |
| `axes` | 9 чисел, необязательно | строки x, y, z матрицы поворота (`MODEL::mtx`); нет — без поворота |

`[[marker]]` — место без модели, имя и место: `name` (обязательно), `position`, `axes`. Скрипт берёт его по имени через `GetSceneMarker` (3.5) и ставит туда то, что сцена сама не создаёт: игрока, NPC с его ИИ и группами. Направление взгляда — ось z маркера: `ay = atan2(z.x, z.z)`, как у `MSG_CHARACTER_TELEPORT_AY`.

```toml
[[model]]
name = 'onlooker'
geometry = 'characters\pirate'
textures = 'characters'
animation = 'fencer'
action = 'idle_1'
position = [ 5.0, 0.8, 5.0 ]

[[marker]]
name = 'pirate'
position = [ 0.0, 0.0, 1.0 ]
axes = [ -1.0, 0.0, 0.0,  0.0, 1.0, 0.0,  0.0, 0.0, -1.0 ]   # смотрит в -z: ay = π
```

- Числа пишутся с 4 знаками (десятая миллиметра), строки — в одинарных кавычках, чтобы `\` пути остался как есть. Одно и то же место всегда даёт тот же текст, поэтому сцена хорошо смотрится в diff.
- Файл пишет сам модуль (`FormatScene`), а не toml++: так ключи идут в порядке чтения, а числа не растягиваются в `0.80000000000000004`.
- Ошибка разбора (не тот тип, 7 чисел в `axes`) — вся сцена не грузится, в `system.log` строка `Scene: …` с причиной. Модель, файла которой нет, пропускается: `Scene: no model …`. Нет скелета или действия — модель остаётся в позе из файла: `Scene: no animation …` / `Scene: no action …`. У маркера без `name` — ошибка разбора.
- Масштаба нет: оси читаются как есть, но гизмо их не тянет.

`[[sound]]` — источник звука: маркер, который играет файл по кругу в своей точке (проверено в `demos/scene`).

| Ключ | Тип | Что |
|---|---|---|
| `name` | строка | имя в окне Scene |
| `sound` | строка | файл от `resource/sounds`, через `\` (`'demo\waves.wav'`); имя без `\` сначала ищется среди алиасов звука |
| `volume` | число, необязательно | 0–1, нет — 1.0 |
| `distance` | 2 числа, необязательно | до первой (м) громкость полная, дальше падает, за второй звука нет; нет — `[2.0, 30.0]` |
| `position` | 3 числа, необязательно | место; `axes` у звука не пишутся |

```toml
[[sound]]
name = 'surf'
sound = 'demo\waves.wav'
volume = 0.8
distance = [ 3.0, 30.0 ]
position = [ 0.0, 0.5, 7.0 ]
```

Звук играется через `SoundPlay` с `PCM_3D`, `looped = true`, `minDistance`/`maxDistance` из `distance`, и останавливается `SoundStop`, когда сцена выгружается. Нет файла — источник пропускается: `Scene: no sound …`. Без `sound` — ошибка разбора `sound "x" has no sound`.

## 4.17 glTF 2.0 → `.gm` и `.tx` (tools/gltf2gm/gltf2gm.py)

Модели из Blender (File → Export → glTF 2.0), Kenney, Sketchfab приходят как `.glb` (один файл) или `.gltf` (JSON и рядом `.bin` и картинки). Конвертер переводит их в статические модели движка без Blender:

| glTF | движок |
|---|---|
| файл `<имя>.glb` | `resource/models/<папка>/<имя>.gm`, грузится как `"<папка>\<имя>"` |
| примитив меша (одна группа треугольников, один материал) | объект `.gm`; у каждого не больше 65 535 вершин |
| узлы сцены (TRS или `matrix`) | запечены в вершины: модель приходит с местами частей, без иерархии |
| `baseColorTexture` | `resource/textures/<папка>/<картинка>.tga.tx`, одна на все модели папки |
| `baseColorFactor` | множитель цвета текстуры; без текстуры — текстура 4×4 `color_rrggbb` |
| `KHR_texture_transform` | сдвиг и масштаб UV |
| `doubleSided` | грани дублируются с обратным обходом: движок отсекает задние грани |
| нет `NORMAL` | плоские нормали по граням |

- **Системы координат.** У glTF правая тройка, у D3D левая, у обоих y вверх и единица — метр. Конвертер зеркалит x (позиции, нормали) и меняет обход треугольников, поэтому модель выглядит как в Blender и смотрит туда же (+z). Без зеркала модель вывернута: видны задние грани, левое и правое поменяны.
- Нормали узлов с масштабом переводятся обратной транспонированной матрицей.
- Не читаются: скин, морфы, анимации, карты нормалей и металла. Для персонажей с костями — `tools/blender-gm-export` (аддон Blender, нужны локаторы и `.tga`).
- BSP строится по умолчанию (`--no-bsp` — без него): по нему попадают `MODEL::Trace`, щелчок в редакторе, камера и ядра.
