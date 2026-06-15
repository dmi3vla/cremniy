<div align="center">

[![Community](https://img.shields.io/badge/Community-Telegram-blue?logo=telegram&style=flat-square)](https://t.me/cremniy_com)

[English](CONTRIBUTING.md) • Русский
	
</div>

# Контрибьюция

Спасибо за ваш интерес к проекту Cremniy.  
Любая помощь в улучшении проекта приветствуется.

## Способы контрибуции

Вы можете помочь несколькими способами:

- сообщать об ошибках (создайте новый **Issue** по шаблону `Bug report`)
- предлагать новые функции (создайте новый **Issue** с тегом `idea`)
- улучшать документацию
- отправлять pull request ([подробнее](CONTRIBUTING_ru.md#pull-requests))

## План развития

Все текущие **задачи** и **планы** развития проекта **собраны** в [дорожной карте](ROADMAP_ru.md).  
Перед созданием Issue или PR **рекомендуем посмотреть**, что уже запланировано, чтобы **не дублировать работу**.

## Языковая политика

Чтобы проект оставался доступным для международных участников, **все Issues, Pull Requests и сообщения коммитов должны быть написаны на английском языке.**

## Работа с ветками

В основном репозитории поддерживаются только две ветки:

- **main**: стабильная версия проекта. Всегда содержит готовый к использованию код.
- **dev**: ветка активной разработки. Здесь создаются и тестируются новые фичи для следующего релиза. После завершения разработки фичи, **dev** вливается в **main** - выпускается MINOR-релиз.

Все остальные ветки (`feature/...`, `fix/...`) создаются **в вашем форке**, когда вы работаете над задачей или багфиксом:

- **feature/...**: ветки для новых функций (создаются от `dev`). После завершения работы создается PR в `dev`.
- **fix/...**: ветки для исправления багов (создаются от `main`). После завершения работы создается PR в `main`. После мержа в `main` багфикс также мержится в `dev`, чтобы изменения попали в разрабатываемую версию.

## Pull requests

### Требования

- Pull request должен решать **одну конкретную задачу** или группу тесно связанных задач
- Не объединяйте в одном PR **разные изменения** (например: новые функции, рефакторинг и фиксы одновременно)
- Крупные изменения **разбивайте на несколько** отдельных PR
- Свяжите PR с задачей, если таковая существует ([ниже подробнее](CONTRIBUTING_ru.md#%D1%81%D0%B2%D1%8F%D0%B7%D1%8B%D0%B2%D0%B0%D0%BD%D0%B8%D0%B5-pr-%D1%81-%D0%B7%D0%B0%D0%B4%D0%B0%D1%87%D0%B0%D0%BC%D0%B8))

### Отправка

1. Сделайте fork репозитория
2. Создайте новую ветку от соответствующей базовой ветки:
   - `dev` для новых функций (feature)
   - `main` для исправления багов (fix)
3. Внесите ваши изменения
4. Синхронизируйте вашу ветку с базовой веткой (`dev` или `main`) и решите конфликты, если они есть
5. Создайте pull request в соответствующую ветку (`dev` или `main`) с понятным описанием или прикреплением Issue

### Связывание PR с задачами

Каждый Pull Request должен **явно указывать, какую задачу или Issue он решает**, если такая [задача](ROADMAP_ru.md) или Issue существует.
Если соответствующей задачи нет, просто опишите изменения в PR.

## Благодарность

Все контрибьюторы будут добавлены в [ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md)  
и упомянуты в конце каждого видео на [YouTube-канале](https://www.youtube.com/@igmunv)

## Архитектура модуля Canvas

Модуль бесконечного канваса находится в `src/ToolTabs/Canvas/` и следует паттерну ToolTab:

```
Canvas/
  canvastab.h/cpp       — главная вкладка, построение графа, подключение сигналов
  canvas_view.h/cpp     — QGraphicsView с pan/zoom/grid
  canvas_layout.h/cpp   — радиальный дерево- layout по директориям
  dependency_parser.h/cpp — async #include парсер (QThread worker)
  gource_animator.h/cpp — чтение git log, управление воспроизведением
  layer_panel.h/cpp     — переключатели типов рёбер (QSettings)
  minimap.h/cpp         — оверлей миникарты 160x120
  nodes/
    file_node.h/cpp     — QGraphicsObject: hover, pulse, context menu
    step_node.h/cpp      — узел шага семантической карты
    cluster_group_node.h/cpp — контейнер кластера
  edges/
    dependency_edge.h/cpp — анимированные рёбра Безье
    connection_edge.h/cpp — рёбра семантических связей
  semantic_map.h/cpp    — модель данных SemanticMap/SemanticCluster/SemanticStep
  semantic_map_store.h/cpp — хранилище карт в .cremniy/semantic_maps/
  semantic_map_utils.h/cpp — утилиты (extractCodeSnippet)
  cluster_layout.h/cpp  — layout кластеров
  digest_panel.h/cpp    — панель digest с кластерами
```

### Запуск тестов канваса

```bash
cd tests && mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=../../third_party/qt-install
cmake --build . && ./test_dependency_parser
```

## Concept Map / Семантический слой

Семантический слой добавляет AI-генерируемую концептуальную карту поверх структурного графа зависимостей в формате `.codemap` (совместимо с Windsurf).

### Поток данных

1. Пользователь вызывает через «Generate Codemap» (Ctrl+Shift+M), переключатель «Структура/Концепт» в toolbar канваса или кнопку «Codemap» в header AI Assistant
2. `IDEWindow::openOrGenerateConceptMap()` проверяет `CodemapStore::exists()` на наличие кэшированного `<project>.codemap` в корне проекта
3. Если нет — генерирует через `GenerateCodemapTool` через `AgentSession::toolRegistry()`
4. `GenerateCodemapTool` отправляет файлы проекта LLM со структурированным системным промтом, валидирует ответ (реальные пути, корректные номера строк, совпадение lineContent), повторяет до 2 раз
5. Валидный результат сохраняется через `CodemapStore::save()` как `<project>.codemap` и отображается через `CanvasTab::showCodemap()`

### Формат .codemap

Файлы хранятся в формате, совместимом с Windsurf, с относительными путями:
- `codemap.h/cpp` — Codemap, CodemapTrace, CodemapLocation, CodemapMetadata
- `codemap_store.h/cpp` — сохранение/загрузка в `<project>.codemap` в корне проекта
- `codemap_utils.h/cpp` — extractCodeSnippet, checkLocationStale

Связи хранятся в поле `mermaidDiagram` (синтаксис Mermaid flowchart), а не в массивах на каждый location.

### Добавление нового поля в CodemapTrace/CodemapLocation

1. Добавьте поле в `codemap.h`
2. Обновите `toJson()`/`fromJson()` — новые поля должны быть опциональными в `fromJson`
3. Обновите системный промт в `GenerateCodemapTool::buildSystemPrompt()`
4. Обновите `validateCodemapJson()`, если поле требует валидации
5. Обновите визуализацию (`StepNode`/`ClusterGroupNode`/`DigestPanel`)

### Тестирование

- `tests/test_codemap.cpp` — сериализация round-trip, конвертация путей, проверка staleness, `CodemapStore`
- `tests/test_generate_codemap_tool.cpp` — логика валидации (без сетевых вызовов, friend-класс через `CREMNIY_TESTING`)

```bash
cd tests && mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=../../third_party/qt-install
cmake --build . && ./test_dependency_parser && ./test_codemap && ./test_generate_codemap_tool
```
