<div align="center">

English • [Русский](ROADMAP_ru.md)
	
</div>

# 🚀 Roadmap

## 🎯 Project Goal
Create a **unified tool for system programming** that eliminates the need for scattered solutions and simplifies the development process.

## 📦 Current Version
[v0.1.3](https://github.com/Cremniy-Project/cremniy/releases/tag/v0.1.3) — basic development environment with:
- Code editor (full support for low-level languages)
- HEX editor (view bytes in RAW format)
- Disassembler (can use `objdump` and `radare2`)
- Calculator for converting between number systems
- Integrated toolset

## 🛠 Short-Term Tasks

### 🐞 Bugs

- [ ] [Display bar in HEX-Editor on Windows](https://github.com/Cremniy-Project/cremniy/issues/33)

### ✨ Improvements and New Tasks

- [ ] 🟡 [Display numbers in different numeral systems on hover](https://github.com/Cremniy-Project/cremniy/issues/28)
- [ ] 🟡 [Improve Terminal](https://github.com/munirov/cremniy/issues/100)
- [ ] 🟢 [Add multilingual support](https://github.com/Cremniy-Project/cremniy/issues/67)
- [ ] 🟢 [Search string across all project files](https://github.com/Cremniy-Project/cremniy/issues/76)
- [ ] 🟢 [Git integration](https://github.com/Cremniy-Project/cremniy/issues/42)
- [x] 🔴 [Implement custom QPlainText for Code Editor](https://github.com/Cremniy-Project/cremniy/issues/56)
- [x] 🔴 [Optimize data storage in QHexView](https://github.com/Cremniy-Project/cremniy/issues/57)
- [x] 🟡 [Improve the design of the search bar in the editor](https://github.com/munirov/cremniy/issues/177)
- [x] 🟡 [Improve Disassembler design](https://github.com/Cremniy-Project/cremniy/issues/55)
- [x] 🟡 [Keyboard scancode reference guide](https://github.com/munirov/cremniy/issues/89)
- [x] 🟡 [Add a data storage unit converter](https://github.com/munirov/cremniy/issues/104)
- [x] 🟡 [Use Breeze icons for files in QTreeView](https://github.com/Cremniy-Project/cremniy/issues/72)
- [x] 🟡 [Implement StatusBar](https://github.com/Cremniy-Project/cremniy/issues/73)
- [x] 🟢 [Pin file tabs (FileTab)](https://github.com/Cremniy-Project/cremniy/issues/75)
- [x] 🟢 [Move files between directories in QTreeView](https://github.com/Cremniy-Project/cremniy/issues/77)

## 🕓 Long-Term Tasks

- [ ] [Foundation of basic architecture for extensibility](https://github.com/Cremniy-Project/cremniy/issues/29)

## 🔮 Future Plans

- Build user projects
- Debugger for running programs
- Memory viewer for running programs

## Infinite Canvas (in progress)

Dependency graph visualization with gource-style animation:

- [x] Phase 1 — Basic canvas (QGraphicsView, pan/zoom/grid)
- [x] Phase 2 — Canvas core (FileNode, DependencyEdge, CanvasTab)
- [x] Phase 3 — Dependency parser (async #include parsing, directory-clustered layout, live updates)
- [x] Phase 4 — Editor integration (click→editor, hover preview, context menu)
- [x] Phase 5 — Gource visualization (git history playback, filter layers, minimap)
- [x] Phase 6 — Tests & documentation

## Concept Map / Semantic Layer (in progress)

AI-generated conceptual overview of the codebase, complementing the structural dependency graph with semantic clusters (windsurf/codemap-style):

- [x] Data model — SemanticMap/SemanticCluster/SemanticStep with JSON serialization, SemanticMapStore (.cremniy/semantic_maps/)
- [x] GenerateSemanticMapTool — agent tool with validation and retry (filePath/lineNumber verification against real files)
- [x] Canvas visualization — ClusterGroupNode, StepNode, ConnectionEdge with labeled connections
- [x] Tools → Concept Map menu entry; Structure/Concept toggle in Canvas toolbar; DigestPanel with collapsible Motivation/Details
- [x] Tests & documentation
