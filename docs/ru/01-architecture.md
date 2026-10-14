# 1. Архитектура

```
engine (src/apps/engine/src/main.cpp)
 ├─ SDL-окно, engine.ini, главный цикл
 └─ core (src/libs/core)
     ├─ Compiler — VM скриптового языка (*.c), события, сообщения
     ├─ EntityManager — сущности (C++-классы) + слои
     ├─ сервисы (renderer, sound_service, string_service, controls, file service…)
     └─ реестр классов __STORM_CLASSES_REGISTRY (LoadClassesTable, core_impl.cpp:343)
libs/* — классы-сущности (sea, ship, xinterface, location, …) и сервисы
```

Скрипт управляет игрой, C++ её исполняет. Скрипт создаёт сущности (`CreateEntity(&obj,"Class")`), кладёт их в слои, передаёт им команды через `SendMessage` и получает события через `SetEventHandler`. Состояние сущностей хранится в **атрибутах** скриптового объекта (дерево `obj.a.b.c`), которые C++ читает напрямую.

## 1.1 Старт (main.cpp → core_impl.cpp)

| Шаг | Где | Что происходит |
|---|---|---|
| 1 | `main.cpp` | Парсинг CLI (`--editor`) и чтение `engine.ini` (`fs::ENGINE_INI_FILE_NAME`, `util/include/fs.h:42`), создание окна SDL |
| 2 | `core_impl.cpp:96` `Init` | Регистрация типов слоёв, загрузка реестра классов |
| 3 | `core_impl.cpp:277` `ProcessEngineIniFile` | `program_directory`, `controls`, `[compatibility] target_version`, `[interface] screen_width/height`, затем `run` → `CreateProgram` + `Run` |
| 4 | `compiler.cpp:526` `Run` | Вызов скриптовой функции `Main()` |
| 5 | Цикл | `CoreImpl::Run` каждый кадр (см. 1.2) |
| 6 | Выход | `ExitProgram()` → скриптовый `ExitMain()` (если есть, `compiler.cpp:5963`) → `core.Exit`; движок также шлёт события `ExitApplication` и `DestroyWindow` (при закрытии окна) |

Если не удалось скомпилировать скрипты, появляется окно «Failed to compile program». При `target_version = teho` скрипт обязан объявить `iScriptVersion = 54128` (`ENGINE_SCRIPT_VERSION`, `core_impl.h:11`), иначе будет «Wrong script version».

## 1.2 Кадр (`CoreImpl::Run`, core_impl.cpp:160)

1. Отладочные клавиши: F7 — `DumpEntitiesInfo`, R — время ×10, Y — время ×0.2.
2. Обновление скриптовых переменных, если они объявлены: `iRealDeltaTime`, `iRealYear/Month/Day`, `fHighPrecisionDeltaTime`.
3. `ProcessEngineIniFile` (выполняется только один раз).
4. `Compiler->ProcessFrame` — доставка отложенных `PostEvent`.
5. `ProcessEvent("frame")` — скриптовое событие `frame`.
6. `ProcessStateLoading` (загрузка сохранения), затем `ProcessRunStart` у сервисов.
7. `ProcessExecute` (сущности execute-слоёв), затем `ProcessRealize` (realize-слоёв, рендер).
8. Steam callbacks, `Controls->Update`, `ProcessControls` → события `"Control Activation"` / `"Control Deactivation"` с именем контрола (`core_impl.cpp:243`).
9. `NewLifecycle` у EntityManager (удаление помеченных сущностей), `ProcessRunEnd`.

Без фокуса окна и без `run_in_background=1` цикл не крутит кадры, а спит по 50 мс (`main.cpp`).

## 1.3 Слои (`S/src/libs/shared_headers/include/shared/layers.h`)

| ID | Имя | Назначение |
|---|---|---|
| 0 / 1 | EXECUTE / REALIZE | основной мир (суша, локации) |
| 2 / 3 | SEA_EXECUTE / SEA_REALIZE | море |
| 4 / 5 | INTERFACE_EXECUTE / INTERFACE_REALIZE | GUI (`xinterface`) |
| 6 / 7 | FADER | затемнения |
| 8 / 9 | LIGHTER | редактор освещения |
| 10 / 11 | VIDEO | видео |
| 12 / 13 / 14 | EDITOR_REALIZE / INFO_REALIZE / SOUND_DEBUG_REALIZE | служебные |
| 15–28 | SEA_REFLECTION, SUN_TRACE, SAILS_TRACE, HULL_TRACE, ISLAND_TRACE, SHADOW, BLOOD, RAIN_DROPS… | трейс-слои и спец-проходы |

В скрипте слой задаётся числом: `LayerAddObject(layerId, &obj, priority)` (3 аргумента). Из скрипта константы доступны через `#include "storm-engine\layers.h"`. Старый `_doc_script/syntax.txt` из TEHO, где слои адресуются по имени, **устарел**.

## 1.4 Версии совместимости

`engine_version.hpp`: `UNKNOWN, SEA_DOGS, POTC, CT, COAS, TEHO, LATEST`. Версия задаётся в `engine.ini` → `[compatibility] target_version = sd|potc|ct|coas|teho|latest` (`core_impl.cpp:26`). От неё зависят базовое разрешение интерфейса (640x480 для potc и ниже, иначе 800x600, `core_impl.cpp:1080`), расчёт высоты шрифта (`font.cpp:~150`) и проверка `iScriptVersion`. Из скрипта версию можно узнать функцией `GetEngineVersion()`.
