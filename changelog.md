# Changelog

All notable changes to Geode Mod Builder are documented in this file.

## v0.1.0

Initial release.

### Added

- In-game main-menu button that opens the builder.
- Project manager: create, open, save, Save As, delete and list projects, with
  delete confirmation and recent-project tracking.
- Project metadata: name, mod ID, developer, version, target Geode and Geometry
  Dash versions, platforms, description and icon.
- Versioned JSON project storage with safe (temporary-file) writes and handling
  of corrupted files and missing assets.
- Visual editor: element tree, canvas preview, properties panel, action editor
  and validation/status area.
- Element types: Button, Label/Text, Sprite/Image, Popup, Panel/Container,
  Screen/Menu, Checkbox, Slider, Text Input and Close Button.
- Popup builder with message, confirmation, settings, info and custom templates.
- Action and event editor covering open/close popup, open screen, change label
  text, show/hide, toggle, set enabled, set opacity, project setting changes,
  notifications and chaining actions.
- Undo/redo history for structural, property and action changes.
- Asset manager with import, rename, delete, texture assignment and missing-file
  checks; original files are never modified.
- Project validator producing English, user-facing error messages.
- Code generator emitting a real Geode C++ project (`mod.json`,
  `CMakeLists.txt`, `src/*.cpp`/`*.hpp`, `resources/`, `about.md`).
- Build manager with SDK/toolchain detection, safe command execution and
  compiler output reporting.
- GitHub Actions build workflow for Windows, macOS, Android32 and Android64,
  with tag-triggered releases.
