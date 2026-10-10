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

## CI & release
- GitHub Actions `.github/workflows/build.yml`: builds Windows/macOS/Android32/
  Android64 via `geode-sdk/build-geode-mod`, and on a `v*` tag creates/updates
  a Release with the combined `.geode` (all platforms in one file).
- CI runs on push to `master`/`main` and on the Geode version bump, so the
  release asset is rebuilt. Full mod compilation is verified in CI, not here.
- Release: https://github.com/normanrus/geode-mod-builder/releases/tag/v0.1.0

## UI notes (GD/cocos2d specifics)
- `ButtonSprite`, `CCMenuItemExt`, `CCMenuItemSpriteExtra`, `CCMenuItemToggler`
  live at GLOBAL scope in GD bindings, NOT inside `cocos2d`.
- `CCMenu` must only contain `CCMenuItem` children; putting labels/sprites/
  TextInput in a `CCMenu` breaks touch handling. Use a plain `CCNode` for
  read-only content (e.g. CanvasPreview world).
- `CCMenu::alignItemsInColumns(...)` is varargs terminated by a trailing `0`.
- `geode::Popup::init/onClose/setTitle` are protected: call `Popup::init` from
  the subclass and use `createQuickPopup` for confirmations. For a light custom
  panel, prefer a plain `CCLayer` over `Popup`.

## Env quirks
- `unzip` is missing -> use `python3 -c "import zipfile; ..."`.
