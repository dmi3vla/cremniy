<div align="center">

[English](ROADMAP.md) • Русский
	
</div>

# 🚀 Дорожная карта

## 🎯 Цель проекта
Создать **единый инструмент для системного программирования**, устраняющий необходимость использования разрозненных решений и упрощающий процесс разработки

## 📦 Текущая версия
[v0.1.3](https://github.com/Cremniy-Project/cremniy/releases/tag/v0.1.3) — базовая среда разработки с:
- редактором кода (полный набор низкоуровневых языков)
- HEX-редактором (просмотр байтов в RAW формате)
- дизассемблером (может использовать `objdump` и `radare2`)
- калькулятором для преобразования в разные системы счисления
- и связанностью инструментов

## 🛠 Ближайшие задачи

### 🐞 Баги

- [ ] [Отображение полоски в HEX-Editor на Windows](https://github.com/Cremniy-Project/cremniy/issues/33)

### ✨ Улучшения и новые задачи

- [ ] 🟡 [Отображение чисел в разных системах счисления при наведении](https://github.com/Cremniy-Project/cremniy/issues/28)
- [ ] 🟡 [Улучшить терминал](https://github.com/munirov/cremniy/issues/100)
- [ ] 🟢 [Добавить многоязычность](https://github.com/Cremniy-Project/cremniy/issues/67)
- [ ] 🟢 [Поиск строки по всем файлам проекта](https://github.com/Cremniy-Project/cremniy/issues/76)
- [ ] 🟢 [Работа с Git](https://github.com/Cremniy-Project/cremniy/issues/42)
- [x] 🔴 [Реализация кастомного QPlainText для Code Editor](https://github.com/Cremniy-Project/cremniy/issues/56)
- [x] 🔴 [Оптимизировать хранение данных у QHexView](https://github.com/Cremniy-Project/cremniy/issues/57)
- [x] 🟡 [Улучшить дизайн формы поиска строки в редакторе](https://github.com/munirov/cremniy/issues/177)
- [x] 🟡 [Улучшение дизайна Disassembler](https://github.com/Cremniy-Project/cremniy/issues/55)
- [x] 🟡 [Справочник по скан-кодам клавиш](https://github.com/munirov/cremniy/issues/89)
- [x] 🟡 [Добавить конвертер единиц хранения данных](https://github.com/munirov/cremniy/issues/104)
- [x] 🟡 [Использование иконок Breeze для файлов в QTreeView](https://github.com/Cremniy-Project/cremniy/issues/72)
- [x] 🟡 [Реализовать StatusBar](https://github.com/Cremniy-Project/cremniy/issues/73)
- [x] 🟢 [Закрепление вкладки файла (FileTab)](https://github.com/Cremniy-Project/cremniy/issues/75)
- [x] 🟢 [Перемещение файлов по директориям в QTreeView](https://github.com/Cremniy-Project/cremniy/issues/77)

## 🕓 Долгосрочные задачи

- [ ] [Основы базовой архитектуры для обеспечения расширяемости](https://github.com/Cremniy-Project/cremniy/issues/29)

## 🔮 Будущие планы

- Сборка проекта пользователя
- Отладчик запущенной программы
- Просмотр памяти запущенной программы

## Infinite Canvas (в процессе)

Визуализация графа зависимостей с анимацией в стиле gource:

- [x] Фаза 1 — Базовый канвас (QGraphicsView, pan/zoom/grid)
- [x] Фаза 2 — Ядро канваса (FileNode, DependencyEdge, CanvasTab)
- [x] Фаза 3 — Парсер зависимостей (async #include, радиальный layout, live-обновления)
- [x] Фаза 4 — Интеграция с редактором (клик→редактор, hover preview, context menu)
- [x] Фаза 5 — Gource-визуализация (git history, фильтры слоёв, миникарта)
- [x] Фаза 6 — Тесты и документация

## Concept Map / Семантический слой (в процессе)

AI-генерируемая концептуальная карта кодовой базы, дополняющая структурный граф зависимостей семантическими кластерами (в стиле windsurf/codemap):

- [x] Модель данных — Codemap/CodemapTrace/CodemapLocation в формате .codemap (совместимо с Windsurf)
- [x] GenerateCodemapTool — инструмент агента с валидацией, retry, проверкой lineContent
- [x] Визуализация на канвасе — ClusterGroupNode, StepNode, ConnectionEdge (связи из mermaidDiagram)
- [x] Пункт меню «Codemap»; переключатель «Структура/Концепт» в toolbar; DigestPanel с per-trace Motivation/Details
- [x] CodemapStore — <project>.codemap в корне проекта (переносимые относительные пути)
- [x] Тесты и документация
