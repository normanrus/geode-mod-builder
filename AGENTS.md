# AGENTS.md — Geode Mod Builder

## Project
Geode SDK mod (C++23, Geode v5.11.1, GD 2.2081) that is a visual UI builder:
it edits a JSON project model and generates a real, compilable Geode mod.

## Build & verification
- The container has NO cmake/ninja/clang/geode CLI. Full mod compilation and
  `geode build` are NOT possible here. Do not claim otherwise.
- Core logic is Geode-free and tested with the host compiler:
  `g++ -std=c++23 -I src -o /tmp/core_tests tests/core_tests.cpp src/core/*.cpp`
  then run `/tmp/core_tests`.
- UI sources are syntax-checked against a Geode/cocos2d stub in /tmp/stubs
  (not committed):
  `g++ -std=c++23 -fsyntax-only -include /tmp/stubs/prelude_main.hpp -I /tmp/stubs -I src src/ui/*.cpp src/main.cpp`

## Layout
- src/core  — Model, Json, ProjectManager, ProjectValidator, CodeGenerator,
  BuildManager, UndoRedoManager (no Geode dependency).
- src/ui    — Geode UI layers (BuilderLayer, EditorLayer, panels).
- src/main.cpp — MenuLayer hook / entry point.
- tests/    — core_tests.cpp.
- resources/ — mod resources.

## Conventions
- Source, identifiers, comments, logs, generated files: English.
- UI defaults to English; developer reports and chat: Russian.
- Model is separated from UI; generated code is deterministic and escaped.
- Never report success without actual verification.

## Env quirks
- `unzip` is missing -> use `python3 -c "import zipfile; ..."`.
