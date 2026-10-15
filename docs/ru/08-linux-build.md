# Сборка Storm Engine под Linux — журнал и инструкция

Машина: Ubuntu 22.04 / Linux Mint 21 (ядро 5.15), AMD Lucienne (встроенная, Vulkan через RADV), clang 14.
Форк: https://github.com/POMXARK/storm-engine (от PiratesAhoy/storm-engine), ветка **`linux-build`**.
Старые ветки `fix/linux-build` и `feature/miniaudio-sound` — то же содержимое со старыми сообщениями коммитов, не использовать.

## 0. Где лежит

Всё лежит в рабочем каталоге `$W` (у автора — на HDD, системный диск почти полон):

```
$W/
├── env.sh          # окружение сборки (source перед любой командой)
├── storm-engine/   # исходники (ветка linux-build)
├── build/          # каталог сборки (Ninja), configure.log
├── conan2/         # CONAN_HOME — кэш пакетов Conan (не в ~/.conan2)
└── tools/          # glslang (bin/glslangValidator)
```


## 1. Что установлено

### Через pip --user (без sudo, в `~/.local/bin`)
| Пакет | Версия | Зачем |
|---|---|---|
| conan | 2.33.0 | зависимости C++ |
| cmake | 3.31.10 | системный 3.22 не поддерживает `CMAKE_PROJECT_TOP_LEVEL_INCLUDES` (нужен ≥ 3.24, им подключается cmake-conan) |

```bash
pip3 install --user "conan>=2,<3" "cmake>=3.28,<4"
```

### В venv на HDD (без sudo, `$W/venv-economy`)
Симулятор экономики игры (`corsairs-remake/economy`): подбор баланса и тесты. numpy, pandas, matplotlib — системные (`--system-site-packages`).
| Пакет | Версия | Зачем |
|---|---|---|
| optuna | 4.1.0 | подбор параметров рынка |
| pytest | 8.3.3 | тесты симулятора |

```bash
python3 -m venv --system-site-packages $W/venv-economy
$W/venv-economy/bin/pip install optuna==4.1.0 pytest==8.3.3
```

### Скачано вручную
- glslang (KhronosGroup, `main-tot`, linux-x86_64-release) → `tools/bin/glslangValidator` — нужен meson'у для сборки dxvk-native. Аналог `apt install glslang-tools`.
- PowerShell 7.4.6 (github.com/PowerShell/PowerShell, `powershell-7.4.6-linux-x64.tar.gz`) → `tools/pwsh/pwsh`, без sudo — проверка `run.ps1` демо на Linux. Для сборки не нужен.

```bash
curl -sL https://github.com/KhronosGroup/glslang/releases/download/main-tot/glslang-main-linux-x86_64-release.tar.gz | tar xz -C tools
```

### Через apt (sudo)
Перед установкой пришлось починить прерванный dpkg (было незавершённое обновление ядра):
```bash
sudo dpkg --configure -a
```
Заголовки X11/XCB — их требует рецепт Conan `xorg/system` (для SDL и dxvk):
```bash
sudo apt install libx11-xcb-dev libfontenc-dev libxaw7-dev libxmu-dev libxmuu-dev libxpm-dev \
  libxres-dev libxss-dev libxt-dev libxv-dev libxcb-glx0-dev libxcb-render-util0-dev libxcb-xkb-dev \
  libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev libxcb-randr0-dev libxcb-shape0-dev \
  libxcb-sync-dev libxcb-xfixes0-dev libxcb-xinerama0-dev libxcb-dri3-dev libxcb-cursor-dev \
  libxcb-dri2-0-dev libxcb-present-dev libxcb-composite0-dev libxcb-ewmh-dev libxcb-res0-dev \
  libxcb-util-dev
```
SDL2 для dxvk-native (meson ищет системный `libSDL2.so`, рантайм-пакета мало):
```bash
sudo apt install libsdl2-dev
```
Уже было в системе: clang 14, gcc 11, ninja, meson 0.61, libvulkan-dev, libgl-dev, libegl-dev, uuid-dev, libx11-dev, libffi-dev.

## 2. Окружение (`env.sh`)

```bash
export W=/path/to/remake-linux   # рабочий каталог
export CONAN_HOME=$W/conan2
export PATH=$W/tools/bin:$HOME/.local/bin:$PATH
export CC=clang CXX=clang++
```
Профиль Conan: `conan profile detect --force` (clang 14, libstdc++11).

## 3. Получение исходников

```bash
cd $W
git clone --recursive https://github.com/POMXARK/storm-engine.git   # --recursive: окна редактора, submodule storm-editor
cd storm-engine && git checkout linux-build
# upstream для обновлений:
git remote add upstream https://github.com/PiratesAhoy/storm-engine.git
```

## 4. Сборка

```bash
. $W/env.sh
mkdir -p $W/build && cd $W/build
cmake ../storm-engine -G Ninja -DCMAKE_BUILD_TYPE=Debug -Wno-dev   # Conan ставит/собирает зависимости
ninja dependencies   # dxvk-native (DX9 → Vulkan), meson + системный SDL2
ninja                # движок
```

Модули игры (её окна редактора) добавляет `-DSTORM_EXTRA_MODULES="<папка>;<папка>"`, см. 6.21. Для corsairs-remake: `-DSTORM_EXTRA_MODULES=$W/corsairs-remake/editor`.
Кириллица в окнах редактора — шрифт из `[editor] font` в `engine.ini` (6.21); пакетов не требует, TTF лежит в репозитории игры.
Звук: по умолчанию на Linux — **miniaudio** (`-DSTORM_SOUND_BACKEND=miniaudio`), на Windows — FMOD. См. п. 7.
Первый configure долгий: Conan собирает из исходников openssl, SDL, wayland, sentry и др.
Гизмо редактора — Conan-пакет `imguizmo/cci.20231114` (MIT). Он требует imgui 1.90.5, движку нужна ветка docking: `conanfile.py` подключает `imgui/1.90-docking` с `force=True`, и Conan собирает imguizmo из исходников под неё.

## 5. Что сломалось и как исправлено

Коммиты в `linux-build`: 8a2c962, 606dc6b, 5a32ef4, e1bc8a5, 9d9a8f1, ec158da, da22846; звук — ff41d25 в `miniaudio-sound` (туда же влита `linux-build`), smoke-тест — 65544a7, демо интерфейса — 79f72f6 → `demos/interface` (3423d2f), мышь — ed75347 (влита в `miniaudio-sound` 89fb36e), демо 3D-модели — 67be69d → `demos/model`, нормали скиннинга — 64a0b93 (влита dc11838), общие генераторы ассетов — 14a6cd8 → `demos/common`, демо персонажа — 8ca9757 → `demos/character`, тень без погодных скриптов — 0809a70 (влита de6ee73), демо локации — 2b9bd5b → `demos/location`.

Linux-порт сделан в 2022 (issue storm-devs/storm-engine#348), но с тех пор Linux-сборку никто не запускал (в 2026 в форке PiratesAhoy гоняется только Windows CI).

| # | Ошибка | Причина | Исправление |
|---|---|---|---|
| 1 | `Unknown CMake command "conan_add_remote"` | CMake 3.22 < 3.24 | CMake 3.31 через pip |
| 2 | `Conflict between openssl/[>=3 <4] and openssl/1.1.1n` | старый пин 2022 г., а libcurl 8 (через sentry) требует OpenSSL 3 | убран пин из `conanfile.py` |
| 3 | `directx/... not resolved in remotes` | `include(linux)` → `ExternalProject_Add` запускал Conan до `conan_add_remote(piratesahoy)` | `include(linux)` перенесён после добавления remote в `CMakeLists.txt` |
| 4 | `Dependencies options were defined incorrectly` | Conan 1-стиль: опции `sdl`/`libsndfile` в `requirements()` | перенесены в `configure()` |
| 5 | `directx/jun10: Invalid: Only windows is supported` | DX SDK 2010 требовался на всех ОС | только на Windows (`conanfile.py`, `CMakeLists.txt`, `src/libs/renderer/CMakeLists.txt`); на Linux есть своя замена `src/libs/util/include/platform/d3dx9.hpp` |
| 6 | `xorg/system: ... are missing` | нет системных X11/XCB-заголовков | apt (п. 1) |
| 7 | Conan висит на `Error downloading ... ftpmirror.gnu.org` | Clash Verge (TUN, fake-ip `198.18.x.x`) не пропускает `*.gnu.org`; другие зеркала GNU работают | локальное зеркало исходников (п. 6) |
| 8 | `Could NOT find zlib` / `CLI11` | регистр имён в `find_package` (на Windows ФС нечувствительна) | `ZLIB`, `CLI11` (коммит 606dc6b) |
| 9 | `Library 'libfmodL.so' not found in package` | пакет fmod на remote piratesahoy — только Windows; Linux-бинарники storm-devs были на storm.jfrog.io (закрыт) | звуковой бэкенд на выбор: miniaudio (по умолчанию) или FMOD из локального рецепта (п. 7), ветка `miniaudio-sound` |
| 10 | `Cannot find source file IniFile.cpp` | файл называется `iniFile.cpp` | регистр в `configapp` (коммит 5a32ef4) |
| 11 | `meson: library 'SDL2' not found` (dxvk-native) | нет dev-пакета SDL2 | `apt install libsdl2-dev` |
| 12 | `no member named 'format' in namespace 'std'` | libstdc++ 12 без `<format>` (есть с GCC 13) | `ImGui::TextFmt` (на fmt) в `entity_manager.cpp`, `sound_service.cpp` |
| 13 | `'storm\blade\blade.hpp' file not found`, `sprintf_s`, `GetEngineSettings` | Windows-пути в `#include`, MSVC-функция, include под `#ifdef _WIN32` | `/`, `std::snprintf`, include вынесен из `#ifdef` |
| 14 | `imgui_impl_dx9.h not found` (configapp) | конфигуратор целиком на WinAPI | exe `configapp` собирается только на Windows |
| 15 | `no such file: build/lib/lib<module>.a` | путь к архивам зашит в `StormSetup.cmake` | `$<TARGET_FILE:module>` через `target_link_options` |
| 16 | `cannot find -lzlib` | имя библиотеки вместо цели Conan | `ZLIB::ZLIB` в `src/apps/engine/CMakeLists.txt` |
| 17 | при каждом запуске уходит сессия в Sentry | `sentry_init` с зашитым DSN вызывался всегда, даже при `STORM_ENABLE_CRASH_REPORTS=OFF` | без crash reports Sentry не инициализируется, отдельный флаг `sentryInitialized_` (коммит 9d9a8f1) |
| 18 | движок висит, окно чёрное, кадры не идут | окно без фокуса → main loop на паузе (`main.cpp:264`), SIGTERM SDL превращает в quit-событие, которое не обрабатывается | `run_in_background = 1` в `engine.ini`; `timeout -k 5` |
| 19 | `filesystem error: cannot get file size ... resource/techniques/ship/flag.fx` | `ScanResourcePaths` не заходил в симлинки — ссылка `resource/techniques` не индексировалась, путь приводился к нижнему регистру (а файл `Flag.fx`) | `follow_directory_symlink` в `file_service.cpp` (ec158da) |
| 20 | `Can't load font ...` — текст в интерфейсе не рисуется | `FindConfigFile` проверял `exists("resource\ini\fonts.ini")` напрямую, без файлового сервиса | путь резолвится через `fio->ConvertPathResource`, при промахе остаётся исходный (`config.cpp`, da22846) |
| 21 | интерфейс не реагирует на клавиши и мышь | в `engine.ini` нет `controls = pcs_controls` → создаётся пустая заглушка `CONTROLS` | строка в `engine.ini` (это настройка, не баг движка) |
| 22 | свободная камера на 2-м кадре улетает в небо | `PCS_CONTROLS` не инициализировал `nMouseDx/nMouseDy`, а под SDL события прибавляются к ним (мусор ~7.9e8) | обнуление в конструкторе `PCS_CONTROLS` (`pcs_controls.cpp`, ed75347) |
| 23 | анимированные модели освещены неверно, свет «плавает» по частям тела | `VBTransform` умножал нормаль на полную матрицу кости, вместе с переносом (`CMatrix * CVECTOR` / `XMVector3Transform`) | только 3×3: явное произведение на Linux, `XMVector3TransformNormal` на Win (`model.cpp`, 64a0b93) |
| 24 | SIGSEGV в `Shadow::Realize` на первом кадре с персонажем | `core.Event("EWhr_GetShadowDensity")` / `EWhr_GetFogDensity` возвращают null без обработчика в скриптах | проверка на null, плотности по умолчанию как в игре (`shadow.cpp`, 0809a70); в демо — свои обработчики |
| 25 | тень персонажа не рисуется | в сгенерированном `.gm` не было BSP → `GEOM::Trace` сразу возвращает 2 (промах) | `write_gm(..., bsp=True)` строит BSP (не баг движка) |
| 26 | клавиша S ведёт персонажа вперёд | `Character` двигает по направлению взгляда со скоростью из `"Walk speed"` действия | у `back walk`/`back run` скорость отрицательная (не баг движка) |
| 27 | море ярко-зелёное, `SetWeather` в демо моря не доходит до конца | `Sky.isDone` вызывает событие `WhrCalcFogColor` посреди функции; `ProcessEvent` не сохранял указатель исполнения (`BC_CallFunction` сохраняет), и функция обрывалась — `Sea2.WaterColor` оставался дефолтным `(0,255,0)` | `ProcessEvent` сохраняет и восстанавливает функцию, код и IP, если событие пришло во время работы скрипта (`compiler.cpp`, e1c878b) |
| 28 | `process event stack error` при `ExitProgram()` из обработчика клавиши | причина не разобрана; проверка стоит внутри цикла обработчиков, фикс 27 её не трогает (то же в демо локации) | безвредно, процесс завершается; не исправлено |
| 29 | `not set force attack action: "attack_force_3"` / `not set block(parry) action: "parry_3"` в system.log, выпад или парирование иногда не начинается | `Attack` (force) и `Parry` выбирали зону из четырёх независимо от того, сколько действий задано в `actions` | зоны дальше `numAttackForce`/`numParry` отключаются (`character.cpp`, f701891) |
| 30 | редактор (`--editor`) на Linux ничего не рисует | бэкенд ImGui `imgui_impl_dx9` собирался только на Windows | dx9-бэкенд на всех ОС (`conanfile.py`, `src/libs/editor/CMakeLists.txt`): d3d9 на Linux даёт dxvk-native |
| 31 | `use of undeclared identifier 'ZeroMemory'` в `imgui_impl_dx9.cpp` | макрос Win32, в заголовках dxvk-native его нет; макрос с параметрами через `target_compile_definitions` генератор Ninja молча отбрасывает | `-include src/imgui_dx9_posix.h` (memset) только для `imgui_backend` |
| 32 | `ImGui_ImplSDL2_InitForD3D: Assertion 'Unsupported'` | SDL-бэкенд ImGui запрещает D3D вне Windows, хотя сам он только принимает ввод | `ImGui_ImplSDL2_InitForOther` вне Windows; `EndFrame` редактора — до `EndScene`, иначе d3d9 отбрасывает его отрисовку |
| 33 | `cannot find -leditor_tools` после `-DSTORM_EDITOR_TOOLS=OFF` | список модулей `global_modules_list` кэшируется, выключенный модуль остаётся в нём | движок линкует только существующие цели (`cmake/StormSetup.cmake`) |
| 34 | `imguizmo/cci.20231114` тянет `imgui/1.90.5` — конфликт с `imgui/1.90-docking` | рецепт imguizmo пинует обычную ветку imgui | `self.requires("imgui/1.90-docking", force=True)` в `requirements()` (`conanfile.py`) |
| 35 | `Assertion 'PushID/PopID or TreeNode/TreePop Mismatch!'` при выборе модели в Entities | `NODER::ShowEditorTree` делал `PushID` без `PopID` | `PopID` в конце (`src/libs/model/src/node.cpp`) |
| 36 | `RUNTIME ERROR in <файл>(<строка>): Incorrect usage of index on single variable` каждый кадр, хотя скрипт верный | окно редактора читало простую переменную (`gold`) через `VDATA::Get(value, 0)`; ошибку движок приписывает строке, которую скрипт исполнял последней | для простой переменной `Get(value)`, индекс — только если `IsArray()` (`corsairs-remake/editor/src/market_tool.cpp`) |
| 37 | в редакторе на Linux не видно курсора мыши, клики уходят не туда | `sdl_input.cpp` держит мышь в relative-режиме всегда, а `SDLWindow::ShowCursor` вне Windows ничего не делал | `ShowCursor` переключает `SDL_SetRelativeMouseMode` (`src/libs/window/src/sdl_window.cpp`); редактор показывает курсор везде, кроме игры во вьюпорте и полёта камеры (`engine_editor.cpp`) |
| 38 | окно редактора без рамки: не двигается, не растягивается, нет кнопки закрытия | окно игры по умолчанию без рамки (`window_borders=0`) и фиксированного размера; размер back buffer задаётся один раз при старте | с `--editor` окно с рамкой и `SDL_WINDOW_RESIZABLE` (`src/apps/engine/src/main.cpp`, `sdl_window.cpp`); `DX9RENDER::RunStart` сверяет размер окна и делает `ResizeBackBuffer` → `Reset` устройства (`s_device.cpp`), редактор и Weather восстанавливают свои ресурсы и состояния |

## 6. Обход недоступного gnu.org (локальное зеркало для Conan)

Нужные Conan'у исходники с gnu.org: autoconf 2.71, m4 1.4.19, gettext 0.26, bison 3.8.2, automake 1.16.5, libiconv 1.17, libtool 2.4.7.
Скачаны с `mirrors.dotsrc.org/gnu/` в `$W/gnu-mirror/<sha256>` (проверены sha256 из `conandata.yml`), раздаются локально:

```bash
cd $W/gnu-mirror && python3 -m http.server 8765 --bind 127.0.0.1 &
# в $CONAN_HOME/global.conf:
core.sources:download_urls=["http://127.0.0.1:8765/", "origin"]
```
Для каждого архива рядом обязателен файл метаданных `<sha256>.json`, иначе Conan получает 404 и идёт на origin:
```json
{"references": {"libiconv/1.17": ["https://ftpmirror.gnu.org/gnu/libiconv/libiconv-1.17.tar.gz"]}}
```
Conan сначала ищет файл по sha256 на локальном сервере, иначе идёт на оригинальный URL.
Альтернатива: в Clash Verge добавить правило `DOMAIN-SUFFIX,gnu.org,DIRECT` — тогда всё это не нужно.

## 7. Звук: miniaudio или FMOD

Слой `storm::audio` (`src/libs/sound_service/src/audio_backend.h`) повторяет используемое подмножество FMOD Core; реализации — `audio_backend_miniaudio.cpp` и `audio_backend_fmod.cpp`. Выбор — CMake-переменная `STORM_SOUND_BACKEND` (пробрасывается в Conan как `-o sound_backend=...`).

- **miniaudio** (по умолчанию на Linux): MIT, пакет `miniaudio/0.11.21` из ConanCenter, ogg через встроенный `stb_vorbis`. Ничего ставить не надо.
- **FMOD на Linux** (проприетарный, бесплатная регистрация):
  1. Скачать «FMOD Engine» 2.02.x для Linux с https://www.fmod.com/download → `fmodstudioapi202xxlinux.tar.gz`.
  2. `storm-engine/cmake/conan-recipes/fmod-linux/install.sh <tarball>` — кладёт SDK в кэш Conan под той же ссылкой `fmod/2.02.05+1@piratesahoy+storm-engine/stable`.
  3. `cmake ../storm-engine -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSTORM_SOUND_BACKEND=fmod`

## 8. Smoke-тест без файлов игры

`tools/smoke-test/` (ветка `miniaudio-sound`): минимальный `engine.ini` + скрипт `program/smoke.c`. Скрипт создаёт сущность `Sound` (поднимает звуковой сервис и DX9-рендер), играет тон 440 Гц, рендерит 180 кадров и вызывает `ExitProgram()`.

```bash
cd $W && . env.sh
storm-engine/tools/smoke-test/run.sh build/Debug/engine   # → "smoke test passed"
```

Ожидаемо: чёрное окно 800×600 на ~3 с, звучит тон, окно закрывается, код выхода 0. Логи — `~/.local/share/Akella/Sea Dogs/Logs/` (`system.log`, `compile.log` со строками `SMOKE:`).

Известные безвредные сообщения: `process event stack error` в `smoke.c` (ExitProgram внутри обработчика события `frame` — выход посреди события), не найден `resource/techniques` и стартовый шрифт, dxvk `Unhandled render state 26`.

## 9. Демо механик и репозиторий игры

Движок и игра разделены:
- **движок** `POMXARK/storm-engine` — только движок и нейтральные примеры механик в `demos/` (карта механик и статусы — `demos/README.md`). Сейчас: `tools/smoke-test` (окно, рендер, звук, VM) `demos/interface` (XINTERFACE, шрифты, строки, ввод, `.tx`) и `demos/model` (`.gm`-модели, свободная камера через `Scene`, солнце/ambient/туман через `weather` + техника `DLightModel`; модели и текстуры генерирует `make_models.py`) и `demos/character` (скиннинг `.gm`, скелет `.an`, действия `.ani` idle/walk, переключение из скрипта; ассеты — `make_character.py`) и `demos/location` (двор: модель с BSP, патч `.ptc`, персонаж `player` — W/S бег вперёд/назад, Shift шаг, A/D и мышь поворот, `locationcamera` следует, P пишет позицию; ассеты — `make_location.py`) и `demos/sea` (море `sea` с волнами, отражением неба и пеной, скайбокс `Sky`, погода; 1/2/3 — день/закат/шторм; ассеты — `make_sea.py`). Писатели форматов `.tx/.gm(+BSP)/.an/.ptc` и матрос — общие, `demos/common/`.

Тест-запуск: нажатия слать `keydown` → пауза 0.15 с → `keyup` (`xdotool key` иногда теряется между кадрами); процесс движка называется `engine-1`.
- **игра** `corsairs-remake` — `$W/corsairs-remake`: `engine/` (submodule на `miniaudio-sound`), `game/` (engine.ini, скрипты, ini; движок запускается отсюда, т.к. индексирует cwd), `tools/` (скачивание и конвертация свободных ассетов). Запуск: `./run.sh build/Debug/engine`. GitHub: https://github.com/POMXARK/corsairs-remake (remote `origin`).

Ассеты: шрифт Old Standard Bold (SIL OFL 1.1), «Девятый вал» Айвазовского (общественное достояние, Wikimedia Commons). Бинарники в git не кладутся — собираются `make_assets.py`.

## 10. Статус

- [x] Conan-граф разрешается
- [x] GNU-исходники берутся с локального зеркала (проверено логом: `.json` и архив → 200)
- [x] Зависимости Conan собраны
- [x] dxvk-native собран
- [x] Звук без FMOD (miniaudio) — компилируется
- [x] Движок собран: `build/Debug/engine` (ветка `miniaudio-sound`, Debug), тесты util/config/xinterface проходят
- [x] Smoke-тест: окно + рендер (dxvk на RADV) + звук (miniaudio) + скриптовая VM
- [x] Демо интерфейса: меню с текстом, клавиатура и мышь
- [x] Демо 3D-модели: палуба с бочками и ящиками, свободная камера, динамический свет и туман (submodule в `corsairs-remake` → 6daa9b2)
- [x] Демо анимированного персонажа: скиннинг, скелет `.an`, действия `.ani` idle/walk (submodule в `corsairs-remake` → d93c3d3)
- [x] Демо локации: ходьба по патчу, пандус на платформу, упор в стены, камера следует, тень (submodule в `corsairs-remake` → a43387d)
- [x] Демо моря: волны, отражение неба, пена, три погоды (submodule в `corsairs-remake` → 9cceb70)
- [ ] Демо остальных механик (см. `demos/README.md`)
- [x] Опубликовать `corsairs-remake` на GitHub
- [ ] Запуск оригинальной игры (нужны файлы ККС/TEHO — в Steam на этой машине не найдена)
- [ ] Починить `.github/workflows/ci_linux.yml` (там команды Conan 1 при Conan 2.12)
- [ ] Влить `miniaudio-sound` в основную ветку своего форка POMXARK (PR в PiratesAhoy не делаем)

## 11. Сборка на Windows 11

Windows — родная платформа движка: CI (`.github/workflows/ci_windows.yml`) собирает его на `windows-latest` с MSVC. Отличия от Linux: рендер — настоящий Direct3D 9 (D3DX из пакета `directx`), звук по умолчанию — FMOD (`STORM_SOUND_BACKEND=fmod`, пакет с remote piratesahoy), `.fx`-техники компилирует D3DX.

1. **Visual Studio 2022** (Community), нагрузка «Разработка классических приложений на C++». В ней уже есть «C++ CMake tools for Windows» (CMake и Ninja) и Windows SDK. Clang не нужен. Нужна CMake ≥ 3.24 (`conan_add_remote`), в VS 2022 она новее.
2. **Python 3** (python.org, галочка «Add to PATH»), затем `pip install conan==2.12.2 Pillow numpy`. Pillow и numpy нужны генераторам ассетов демо и игры (numpy — морю `make_sea.py`), Pillow — ещё импорту моделей в редакторе.
3. **Git** с submodule:
   ```bat
   git clone --recursive -b miniaudio-sound https://github.com/POMXARK/storm-engine.git
   ```
   Без `--recursive` не будет `src/libs/editor_tools`. Тогда либо `git submodule update --init`, либо `-DSTORM_EDITOR_TOOLS=OFF`.
4. Сборка — в **x64 Native Tools Command Prompt for VS 2022** (там MSVC в PATH):
   ```bat
   conan profile detect
   cd storm-engine
   cmake --preset=default
   cmake --build --preset=debug
   ```
   При первой конфигурации Conan скачает зависимости (несколько ГБ в `%USERPROFILE%\.conan2`). Remote piratesahoy CMake добавит сам. Бинарник лежит в `build\default\Debug\engine.exe`, рядом — `fmodL.dll` и `resource\techniques`.
   Можно и без консоли: открыть папку `storm-engine` в VS 2022 (File → Open → Folder), выбрать пресет `default`/`debug` и startup item `engine.exe`.
5. Звук без FMOD (как на Linux): `cmake --preset=default -DSTORM_SOUND_BACKEND=miniaudio`.
6. Для запуска нужен DirectX 9 runtime (June 2010, `dxwebsetup`): в Windows 11 нет `d3dx9_43.dll`.

**Демо.** У каждого демо рядом с `run.sh` лежат `run.ps1` (те же шаги на PowerShell) и `run.bat` (обёртка над `run.ps1` для двойного щелчка). Без пути к движку берётся `build\default\Debug\engine.exe`:
```bat
demos\scene\run.bat                 :: двойной щелчок или из cmd
demos\scene\run.bat --editor
demos\scene\run.bat D:\build\engine.exe --editor
```
```powershell
.\demos\scene\run.ps1 --editor    # из PowerShell; если скрипты запрещены — запускайте run.bat
```
Общие шаги — в `demos/common/demo.ps1`: Python ищется как `py -3`, `python`, `python3`; шейдеры подключаются junction-ом (не нужны ни права администратора, ни режим разработчика), если не вышло — symlink, затем копия; движок стартует из папки демо, потому что `program\` и `resource\` он ищет от cwd. При ошибке окно `run.bat` не закрывается. Скачивание шрифтов — `fetch_assets.ps1` у dialog и interface (ассеты и так лежат в git). Скрипты проверены через PowerShell 7 на Linux (`run.ps1` запускает scene, trigger, interface, save, sound, gltf); на Windows не проверены. Git Bash с `run.sh` тоже работает, если есть `python3`.

**Сборка одним шагом — `build.bat`** (`build.ps1`) в корне движка, из обычного PowerShell или двойным щелчком: входит в x64 developer shell самой новой Visual Studio (через `vswhere`, подходят и Build Tools, 2022 или 2019), добавляет в PATH папки `Scripts` Python (туда pip кладёт `conan.exe`, но в PATH их часто нет), доставляет pip-ом недостающее — Conan 2.12.2, Ninja ≥ 1.11 и CMake 3.31 (у VS 2019 ninja 1.10 и CMake 3.20), Pillow и numpy, — делает профиль Conan и собирает пресет. Параметры: `-Config Release`, `-ExtraModules <папка модуля игры>`. Неудачная конфигурация без компилятора оставляет кэш, на котором спотыкается следующая; `build.ps1` такой `build\default` удаляет.

**Игра corsairs-remake.** Движок уже лежит в ней submodule `engine/` (тот коммит, с которым игра проверена), отдельный клон `storm-engine` не нужен. `build.bat` в корне игры собирает `engine\` с окнами редактора игры:
```powershell
git clone --recursive https://github.com/POMXARK/corsairs-remake.git
cd corsairs-remake
.\build.bat
.\run.bat --editor
```
Вручную то же — в Developer PowerShell for VS:
```powershell
git clone --recursive https://github.com/POMXARK/corsairs-remake.git
cd corsairs-remake\engine
cmake --preset=default "-DSTORM_EXTRA_MODULES=$PWD\..\editor"
cmake --build --preset=debug
cd ..
.\run.bat --editor
```
`STORM_EXTRA_MODULES` — окна редактора игры (Market); без него игра тоже собирается и запускается. `run.bat` (`run.ps1`) собирает ассеты игры генераторами `engine/demos` (первый раз ~1–2 мин) и запускает `engine\build\default\Debug\engine.exe` в `game\`.

**Частые ошибки при первой сборке:**
- `The source directory "C:/storm-engine" does not exist` → `cmake` запущен не из той папки: путь к исходникам считается от текущей. Конфигурировать из папки движка пресетом (`cmake --preset=default`), а не `cmake ../storm-engine` из папки игры: так CMake пишет `CMakeCache.txt` и `CMakeFiles\` прямо в репозиторий игры, их надо удалить.
- `-DSTORM_EXTRA_MODULES=<this repo>/editor` → `<this repo>` — подстановка, а не синтаксис: PowerShell и cmd читают `<` как перенаправление. Писать полный путь.
- `Building for: Visual Studio 16 2019` → без пресета CMake берёт первый найденный генератор. Пресет задаёт Ninja Multi-Config, а Ninja и MSVC есть в PATH только в **Developer PowerShell / x64 Native Tools Command Prompt for VS 2022**. VS 2019 (MSVC 19.29) не проверялась; если нет VS 2022, бесплатно ставятся Build Tools for Visual Studio 2022 с нагрузкой «Разработка классических приложений на C++».
- `Conan executable not found! Please install conan.` / `conan` не распознано → pip поставил `conan.exe` в `%APPDATA%\Python\Python3xx\Scripts`, которой нет в PATH (pip пишет об этом WARNING). `build.bat` добавляет её сам; вручную — `$env:PATH = "$env:APPDATA\Python\Python314\Scripts;$env:PATH"`.
- `CMake was unable to find a build program corresponding to "Ninja Multi-Config"`, `CMAKE_CXX_COMPILER not set` → обычный PowerShell, не developer shell: нет ни Ninja, ни MSVC в PATH. `build.bat` входит в него сам; вручную — удалить `build\default` и конфигурировать из Developer PowerShell for VS.
- `'C:/Program' is not recognized as an internal or external command` при сборке пакета (`fmt/10.2.1: Error in build() method … cmake.configure()`) → Conan собирает зависимость из исходников и подставляет `tools.cmake:cmake_program` в строку cmd без кавычек, а CMake стоит в `C:\Program Files\CMake`. `CMakeLists.txt` движка теперь сам берёт такой путь в кавычки; удалить `build\default` и повторить `build.bat`.
- `Check for working C compiler … broken`, `ninja: error: CMakeFiles\common.ninja:23: loading 'CMakeFiles\rules.ninja': The system cannot find the file specified` → не версии CMake/ninja (так же падают 3.31 и 1.13). На русской Windows с VS 2019 Build Tools пробная сборка Ninja не получает `rules.ninja`; вероятная причина — русский префикс `/showIncludes` у cl, который CMake пишет в `msvc_deps_prefix`; `VSLANG=1033` без английского языкового пакета не помогает. `build.bat` в этом случае сам перенастраивает `build\default` генератором Visual Studio той же версии (`Visual Studio 16 2019`/`17 2022`, `-A x64`): он не использует ни Ninja, ни `/showIncludes`, а `engine.exe` остаётся в `build\default\<Config>`; следующие запуски берут генератор из кэша. Вручную — `cmake -S . -B build\default -G "Visual Studio 16 2019" -A x64` и `cmake --build build\default --config Debug`.
- `LNK2019 … __imp_InitCommonControlsEx` в `scene-test.exe` → core (окно `tm_list.cpp`) зовёт comctl32, а её линковал только `engine`; теперь core подключает её сам (`PUBLIC`).
- `ModuleNotFoundError: No module named 'numpy'` в `run.bat` → `build.bat` ставит numpy pip-ом `--user`, а `run.ps1` запускал генераторы с `python -I`: изолированный режим не видит пользовательских пакетов. `demo.ps1` теперь зовёт Python без `-I`; иначе — `pip install numpy` без `--user`.
- `run.bat` прогнал генераторы и сразу вернулся, окна нет → `engine.exe` — GUI-программа (`WIN32` в `StormSetup.cmake`), PowerShell не ждёт её после `&`, код выхода теряется. `Start-Demo` теперь запускает движок через `Start-Process -Wait -PassThru` и возвращает его код. Если движок упал — логи в `Документы\My Games\Sea Dogs\Logs\{error,system,compile}.log`.
- `engine exited with -1073741515 (0xC0000135)`, свежих логов нет → Windows не нашла DLL движка, обычно `d3dx9_43.dll`: поставить DirectX End-User Runtime June 2010 (`dxwebsetup.exe` с microsoft.com). `run.ps1` при этом коде называет, какой DLL нет в `System32` и рядом с движком. Старые логи в той же папке могут остаться от другой игры на Storm — смотреть на дату.
- `not next to the engine: mimalloc.dll (or mimalloc-debug.dll)` → `conanfile.py` копировал DLL mimalloc по точному имени `mimalloc-debug.dll`, а prebuilt-пакет Debug называет её иначе. Теперь копируется по маске `mimalloc*.dll`; `build.bat` заново, он переконфигурирует и докопирует.
- Пустая папка `engine\` или `src\libs\editor_tools` → клон без `--recursive`: `git submodule update --init --recursive`.

**Логи** на Windows лежат в `Documents\My Games\Sea Dogs\Logs\` (а не в `~/.local/share`).

**Курсор в редакторе.** На Windows курсор прячет и показывает `::ShowCursor` (`sdl_window.cpp`). На Linux, где ввод держит мышь в relative-режиме, то же делает `SDL_SetRelativeMouseMode` (п. 5, строка 37).

Сборка на Windows здесь не проверялась (`build.ps1` проверен только разбором PowerShell 7): команды взяты из CI и `conanfile.py`, Linux-изменения ветки под `#ifdef`/`if (WIN32)`.
