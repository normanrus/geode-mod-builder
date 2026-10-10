# Geode Mod Builder

![Geode Mod Builder logo](logo.png)

Geode Mod Builder is an in-game visual editor for building simple Geode mods
without writing C++ by hand. You lay out a UI inside Geometry Dash, wire up
actions, preview the result, and the builder generates a real, compilable
Geode C++ project for you.

It is distributed as a Geode mod itself and appears as a button in the main
menu.

## Features

- Project manager: create, open, save, Save As, delete and list projects with
  confirmation and recent-project tracking.
- Visual editor: element tree, canvas preview, properties panel, action editor
  and a status/validation area.
- Supported element types: Button, Label/Text, Sprite/Image, Popup,
  Panel/Container, Screen/Menu, Checkbox, Slider, Text Input and Close Button.
- Popup builder with message, confirmation, settings, info and custom templates.
- Action and event editor (Open Popup, Close Popup, Open Screen, Change Label
  Text, Show/Hide, Toggle, Set Enabled, Set Opacity, set a project setting,
  notify, run another action).
- Undo/redo for add/remove, rename, property, ordering and action edits.
- Asset manager: import, rename, view, delete, assign textures and check for
  missing or unsupported files. Originals are copied, never modified.
- Project validator with English, user-facing error messages.
- Code generator that emits `mod.json`, `CMakeLists.txt`, `src/*.cpp`/`*.hpp`,
  `resources/` and `about.md` for a standalone mod.
- Build manager with SDK/toolchain detection, safe command execution and
  compiler output reporting.
- Projects are stored as versioned JSON in the mod's save directory.

## Building from source

### Requirements

- [Geode SDK](https://github.com/geode-sdk/geode) v5.11.1
- [Geode CLI](https://github.com/geode-sdk/cli)
- CMake 3.21+, Ninja and a C++23-capable compiler (clang recommended)

### Local build

```sh
# Point the Geode SDK at this project's mod.json (Geode v5.11.1, GD 2.2081)
geode sdk install /path/to/geode-sdk

# Configure and build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The resulting `.geode` file is produced in `build/`.

### CI build

The repository builds on every push and pull request via GitHub Actions
(`.github/workflows/build.yml`) for Windows, macOS, Android32 and Android64.
Pushing a tag matching `v*` additionally publishes a GitHub Release with the
combined `.geode` file attached.

## Project layout

- `src/core` - model, JSON, project storage, validation, code generation,
  build manager and undo/redo. No Geode dependency, unit-tested with a host
  compiler.
- `src/ui` - Geode UI layers (main screen, editor, panels, popups).
- `src/main.cpp` - mod entry point (main-menu button).
- `tests` - core test suite.
- `about.md`, `changelog.md`, `logo.png` - mod metadata and in-game page
  assets, packaged into the `.geode`.

## Testing

Core logic is Geode-free and can be built and run locally:

```sh
g++ -std=c++23 -I src -o /tmp/core_tests tests/core_tests.cpp src/core/*.cpp
/tmp/core_tests
```

Full mod compilation requires the Geode toolchain and is performed in CI.

## License

See the repository for licensing information.
