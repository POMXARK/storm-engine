# 3. Скриптовый язык

Это C-подобный язык с атрибутами и событиями, без указателей. Компилятор и VM находятся в `S/src/libs/core/src/compiler.cpp` (около 8000 строк), лексер — в `token.cpp:153`.

## 3.1 Синтаксис

| Категория | Состав |
|---|---|
| Типы | `int`, `float`, `string`, `bool` (= int), `object`, `ref`, `aref`, `ptr` (uint64) |
| Управление | `if/else`, `for`, `while`, `switch/case`, `break`, `continue`, `return`, `goto` + метка `name:` |
| Ссылки | `makeref(rChar, Characters[n])` (ref на объект), `makearef(ar, obj.a.b)` (ref на узел атрибутов) |
| Атрибуты | `obj.a.b = 5;` и динамическое имя `obj.(sName).x`; `CheckAttribute`, `DeleteAttribute`, `GetAttributesNum`, `GetAttributeN`, `GetAttributeName`, `GetAttributeValue`, `CopyAttributes`, `DumpAttributes` |
| Объявления | `extern void F();` (функция в другом файле), `native float F(...);` (функция из C++-библиотеки) |
| Операторы | `= == != > >= < <= += -= *= /= + - * / % ^ ++ -- && \|\| ! &` |
| Комментарии | `//`, `/* */` |
| Имена | **нечувствительны к регистру**: переменная `rumour` и функция `Rumour()` конфликтуют, а компилятор сообщает только `Invalid syntax` на строке объявления. То же для `#define` (`DefTab::FindDef` сравнивает через `iEquals`): при `#define PORT_TICK 250` вызов `PostEvent("Port_Tick", ...)` передаёт пустое имя события, и движок падает в `S_EVENTMSG` на `strlen(nullptr)` без записи в лог |

**Директивы** (token.cpp):

| Директива | Действие |
|---|---|
| `#include "file.c"` | путь относительно `program_directory`; `#include "storm-engine\x.h"` берётся из `<exe dir>/resource/shared/` (compiler.cpp:213) |
| `#define NAME value` | константа |
| `#event_handler(EVENT,"Func");` | статическая подписка на событие |
| `#library "Name"` / `#libriary "Name"` | подключение нативной библиотеки (compiler.cpp:1411) |
| `#hold`, `#file` | служебные |
| `#ifndef` / `#endif` | **считаются комментариями**: include guards безопасны, но условная компиляция не работает |

Заголовки в `resource/shared` (копии `S/src/libs/shared_headers/include/shared/`): `layers.h`, `events.h`, `messages.h`, `sound.h`, `controls.h`, `interface/messages.h` и др.

## 3.2 Встроенные функции (`internal_functions.cpp:120` IntFuncTable)

| Группа | Функции |
|---|---|
| Сущности | `CreateClass`, `CreateEntity`, `DeleteClass`, `DeleteEntities`, `DeleteEntitiesByType`, `IsEntity`, `GetEntity`, `FindEntity`, `FindEntityNext`, `EntityUpdate`, `ClearEntityAP` |
| Слои | `LayerAddObject`, `LayerDelObject`, `LayerDeleteContent`, `LayerSetRealize`, `LayerSetExecute`, `LayerSetMessages`, `LayerFreeze` |
| События/сообщения | `SendMessage`, `Event`, `PostEvent`, `GetEventData`, `SetEventHandler`, `DelEventHandler`, `ClearEvents`, `ClearPostEvents`, `EventsBreak` |
| Контролы | `CreateControl`, `DeleteControl`, `MapControl`, `SetControlFlags`, `SetControlTreshold`, `LockControl` |
| Сегменты/состояние | `LoadSegment`, `UnloadSegment`, `SegmentIsLoaded`, `SaveEngineState`, `LoadEngineState` (скрипт отвечает функциями `OnSave`/`OnLoad`, 6.13), `SaveVariable`, `LoadVariable` |
| Математика | `Rand`, `frnd`, `abs`, `sqrt`, `sqr`, `sin`, `cos`, `tan`, `atan`, `atan2`, `asin`, `acos`, `Pow`, `shl`, `shr`, `and`, `or` |
| Строки/типы | `sti`, `stf`, `fts`, `MakeInt`, `MakeFloat`, `strcut`, `findSubStr`, `strlen`, `GetSymbol`, `IsDigit`, `argb`, `Vartype`, `sort` |
| Массивы/ссылки | `SetArraySize`, `GetArraySize`, `ClearRef`, `TestRef` |
| Прочее | `Trace`, `Breakpoint`, `Stop`, `ExitProgram`, `GetDeltaTime`, `SetTimeScale`, `CheckFunction`, `GetTargetPlatform`, `GetEngineVersion` |
| Конфиги (config_library.cpp) | `LoadConfig`, `FindConfig`, `ListFiles` |

## 3.3 Сообщения: `SendMessage(&entity, "fmt", ...)`

Строка формата описывает аргументы, первый аргумент — обычно код сообщения (`compiler.cpp:2620` CreateMessage).

| Символ | Тип |
|---|---|
| `l` | int |
| `f` | float (int конвертируется) |
| `s` | string |
| `p` | ptr |
| `i` | entity id из object/aref |
| `e` | ссылка на переменную (C++ пишет туда ответ) |
| `a` | атрибуты object/aref |

В C++ (`message.cpp:185`) есть ещё `b`, `w`, `u`, `d`, `c` (CVECTOR). Возвращаемое значение — uint64 (ptr). При неверном формате будет `InvalidMessageFormatError` и ошибка в error.log.

Пример: `SendMessage(&GameInterface, "ls", MSG_INTERFACE_INIT, "resource\ini\interfaces\mainmenu.ini");`

## 3.4 События

| Вызов | Поведение |
|---|---|
| `Event("Name", "fmt", ...)` | синхронно вызывает все обработчики |
| `PostEvent("Name", delay_ms, "fmt", ...)` | откладывает событие (доставка в `ProcessFrame`); параметры передаются только при ≥4 аргументах (compiler.cpp:1824) |
| `SetEventHandler("Name", "Func", flag)` | подписка. `flag` фактически ни на что не влияет: FSTATUS_NEW нигде не проверяется (s_eventtab.cpp:106) |
| `GetEventData()` | внутри обработчика читает параметры по порядку |

Движок сам вызывает обработчики не только из своего цикла, но и посреди выполнения скрипта. Пример: присваивание `Sky.isDone = ""` вызывает `AttributeChanged`, и небо прямо в нём запрашивает у скриптов `WhrCalcFogColor` для каждой вершины полосы тумана. До фикса e1c878b (`compiler.cpp`, `ProcessEvent`) VM после такого обработчика не восстанавливала выполняемую функцию: функция, где стояло присваивание, молча обрывалась, следующие строки (например, все `Sea.Sea2.*`) не выполнялись. Признак этой ошибки: Trace после присваивания не выводится, а море зелёное (дефолтный цвет). Обработчик, который движок ждёт синхронно (`WhrCalcFogColor`, `EWhr_GetShadowDensity`, `EWhr_GetFogDensity`), нужно назначить **до** создания сущности или присваивания, иначе вернётся 0.

Системные события: `frame` (каждый кадр), `Control Activation` / `Control Deactivation` (параметр — имя контрола), `ievnt_command` (GUI, `"ss"`: команда, узел), `exitCancel`, `ExitApplication`, `DestroyWindow`.

## 3.4.1 Перезагрузка скриптов на лету

`core.ReloadScripts(errors)` (`Compiler::ReloadScripts`, вызывает окно Assets редактора) компилирует все загруженные сегменты заново с диска, не останавливая игру:

- Сегмент компилируется под новым id рядом со старым. Коды функций, переменных и `#define` в таблицах стабильны по имени, поэтому байткод других сегментов, `SetEventHandler` и `PostEvent` продолжают ссылаться на правильные записи.
- Значения глобальных переменных (`DATA`) переносятся из старого сегмента, если тип и «массивность» совпали: на их атрибуты указывают сущности. Иначе старое значение откладывается до `Release` (не удаляется сразу — на него может ссылаться выполняющийся код), а переменная получает новое.
- Нативные функции (`#library`) восстанавливаются: библиотека грузится один раз, и повторный `#library` их не привязывает.
- Обработчики событий переезжают на новый id сегмента.
- При ошибке компиляции всё откатывается: таблица функций, переменные, defines, и работает старый код. Ошибки (`SetError`) собираются в `errors` с файлом и строкой.

`Main()` не вызывается. Сегменты, выгруженные через `UnloadSegment`, пропускаются.

## 3.5 Нативные библиотеки (`#library`)

| Библиотека | Где | Функции |
|---|---|---|
| `dx9render_script_libriary` | renderer/src/s_device.cpp:205 | `SetTexturePath`, `RPrint`, `SetGlowParams`, `GetTexture`, `ReleaseTexture` |
| `script_libriary_test` | script_library/src/vano_files.cpp | `Bring2Range`, `Bring2RangeNoCheck`, `Degree2Radian`, `Clampf`, `rdtsc_b/e` |
| `SteamApiScriptLib` | steam_api | `GetSteamEnabled`, `DLC*`, `SetAchievement`… |
| location | location/src/location_script_lib.cpp:316 | `NativeFindCharacter`, `NativeFindLocation`, `NativeFindLoadCharacter`, `ReloadProgressStart/Update/End`, `SetReloadProgress*`, `SystemDelay`, `ExecuteTechnique`, `GetNextLineString` |
| rigging | rigging | `funcGetSailSpeed`, `RandomHole2Sail`, `DeleteOneSailHole`, `GetAssembledString`, `ShipSailState` |
| `SceneLibrary` | scene/src/storm/scene/scene_library.cpp | `LoadScene(name)` — сцена из `resource/scenes/<name>.scene` (4.16, 6.21); библиотека `SceneMarkers`: `GetSceneMarker(name, &place)` — маркер загруженной сцены: `place.x/y/z/ay`, 1 или 0, если маркера нет; библиотека `SceneObjects` (6.25): `SceneObjectAction(name, action)`, `SceneObjectShow(name, show)`, `SceneObjectMove(name, x, y, z, ay)` — 1, если объект сцены есть; `CheckSceneTriggers(who, x, y, z)` — в скольких зонах `who`, переходы границ — события `SceneTriggerEnter`/`SceneTriggerExit` с `(string trigger, string who)` |
| xinterface (SCRIPT_INTERFACE_FUNCTIONS) | xinterface/src/str_service.cpp:1574 | `LanguageOpenFile`, `LanguageConvertString`, `LanguageCloseFile`, `LanguageGetLanguage`, `LanguageSetLanguage`, `XI_ConvertString`, `XI_MakeNode`, `XI_DeleteNode`, `XI_WindowShow/Disable/IsWindowEnable/AddNode`, `XI_IsKeyPressed`, `XI_CreateFolder`…, `DialogAssembleStr`, `AddControlTreeNode` |

**Порядок важен: `native` до `#library`.** На `#library` (или `#libriary`) компилятор сразу создаёт библиотеку и зовёт её `Init()` (`compiler.cpp`, `INCLUDE_LIBRIARY`), а `SetScriptFunction` привязывает только функции, уже объявленные в скрипте. Объявление после `#library` даёт `native function 'X' not declared` в `error.log` и затем `Undeclared identifier: X` на вызове. Библиотека грузится один раз за компиляцию: повторный `#library` в другом файле пропускается, поэтому все её `native` объявляют до первого подключения.

```c
native int LoadScene(string name);
native int GetSceneMarker(string name, ref place);
#library "SceneLibrary"
#library "SceneMarkers"
```

Библиотека требует объявить **все** свои функции: `SetScriptFunction` на необъявленную тоже пишет `native function 'X' not declared`. Поэтому функцию, которую добавили в уже используемый модуль, выносят в отдельную библиотеку (так `GetSceneMarker` живёт в `SceneMarkers`, а скрипты только с `LoadScene` работают как раньше, а функции объектов и зон — в `SceneObjects`).

Имена в скрипте не различают регистр, и с ключевыми словами тоже: функция `GoTo` — это `goto`, переменная `target` рядом с объектом `Target` — повтор (`Duplicate variable name`). Ошибка ключевого слова видна не сразу: скрипт компилируется, а при запуске падает `stack 'read' error` (07).
