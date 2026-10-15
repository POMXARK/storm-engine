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
