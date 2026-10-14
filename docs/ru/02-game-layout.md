# 2. Запуск и минимальная структура игры

## 2.1 Дерево каталогов

Движок запускается из каталога игры (cwd). Рабочие минимальные игры — демо в `demos/` (по одному на механику, см. [06-recipes.md](06-recipes.md)): каждое `run.sh` генерирует ассеты в `resource/` и запускает движок из каталога демо.

```
<game>/
├── engine                     # бинарник (или симлинк)
├── engine.ini                 # конфиг движка
├── program/                   # скрипты (*.c, *.h) — program_directory
│   └── main.c                 # run = main.c, функция Main()
└── resource/
    ├── shared/                # заголовки движка для #include "storm-engine\..."
    │                          #   (копируются сборкой рядом с exe; берутся из <exe dir>/resource/shared)
    ├── techniques/            # *.fx шейдеры/техники (копируются сборкой, CMakeLists.txt:97)
    ├── ini/
    │   ├── fonts.ini          # startFontIniFile
    │   ├── interfaces/        # interfaces.ini, defaultnode.ini, pictures.ini, <экран>.ini
    │   ├── texts/             # language.ini, <язык>/*.ini (строки)
    │   ├── aliases/*.ini      # звуковые алиасы
    │   └── sound_scheme.ini   # фоновые звуковые схемы
    ├── textures/              # *.tx (имя.tga.tx)
    ├── models/                # *.gm, *_<свет>.col
    ├── animation/             # *.ani (ini) + *.an (бинарь)
    ├── sounds/                # wav/mp3/ogg/flac
    ├── particles/             # *.xps
    └── videos/
```

Пользовательские данные (stash) лежат отдельно: на Linux — `~/.local/share/Akella/Sea Dogs/` (`SDL_GetPrefPath("Akella","Sea Dogs")`), на Windows — `Documents/My Games/Sea Dogs` (`fs.h`). Подкаталоги (`core/src/storm/engine_settings.cpp`): `Logs`, `SaveData`, `Screenshots`, `Cache` (кэш скриптов), `sentry-db`. Их можно переопределить в `engine.toml` ключами `stash_folder` и `logs_folder`.

## 2.2 Пути и регистр на Linux (`file_service.cpp:473` `ConvertPathResource`)

- Скрипты и ini пишут пути в стиле Windows (`resource\ini\...`, любой регистр).
- Вне Windows движок **один раз** рекурсивно сканирует cwd (следуя симлинкам) и каталог exe и строит словарь путей в нижнем регистре. В словарь попадают только пути, начинающиеся с `program`, `resource`, `save`, и любые `*.ini` (`AddEntryToResourcePaths:431`). `\` заменяется на `/`.
- Новый файл в уже известном каталоге находится через родительский каталог. **Каталоги, созданные после старта, неизвестны** (вывод из кода).
- Файлы, открываемые в обход fio (например, BmFont `.fnt` через `std::ifstream`), регистрозависимы (вывод из кода).

## 2.3 engine.ini — ключи

| Ключ | Секция | Кто читает | Смысл / умолчание |
|---|---|---|---|
| `screen_x`, `screen_y` | — | main.cpp, s_device.cpp | размер окна (1024x768) |
| `full_screen`, `display`, `window_borders`, `title` | — | main.cpp | окно |
| `max_fps` | — | main.cpp | ограничение FPS |
| `logs` | — | main.cpp | 0 — отключить логи |
| `run_in_background`, `sound_in_background` | — | main.cpp | работа без фокуса; звук без фокуса — только вместе с run_in_background |
| `Steam` | — | main.cpp | 1 по умолчанию; для своей игры ставить 0 |
| `program_directory` | — | core_impl.cpp:277 | каталог скриптов (движок сам добавляет `\`) |
| `run` | — | core_impl.cpp | стартовый файл скрипта |
| `controls` | — | core_impl.cpp | класс контролов (`pcs_controls` в TEHO, иначе базовый `CONTROLS`) |
| `target_version` | `[compatibility]` | core_impl.cpp:26 | sd/potc/ct/coas/teho/latest |
| `screen_width`, `screen_height` | `[interface]` | core_impl.cpp:1080 | виртуальное разрешение GUI |
| `debuginfo`, `codefiles`, `runtimelog`, `cache_mode` | `[script]` | compiler.cpp:432 | отладка VM, кэш скриптов. `tracefiles` **не используется** (закомментирован, :469) |
| `fade_time` | `[sound]` | sound_service | 0.5 |
| `startFontIniFile`, `font` | — | s_device.cpp | ini шрифтов (`resource\ini\fonts.ini`) и шрифт по умолчанию (`normal`) |
| `show_fps`, `show_exinfo`, `vsync`, `msaa`, `adapter`, `screen_bpp`, `safe_render`, `texture_degradation`, `fov_multiplier`, `NearClipPlane` (0.1), `FarClipPlane` (4000), `screenshot_format` (jpg), `texture_log`, `UseLargeBackBuffer`, `lockable_back_buffer`, `DropVideoConveyor` | — | s_device.cpp:492–612 | рендер |
| `RelativePosX/Y`, `RelativeWidth/Height`, `HorisontalFramesCount`, `VerticalFramesCount` | `[ProgressImage]` | s_device.cpp | анимация загрузки |

Минимальный `engine.ini` (из `demos/interface`):
```ini
full_screen = 0
screen_x = 800
screen_y = 600
max_fps = 60
run_in_background = 1
Steam = 0
program_directory = program
run = menu.c
startFontIniFile = resource\ini\fonts.ini
font = interface_normal
[sound]
fade_time = 0.5
```
