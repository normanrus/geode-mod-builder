# Geode Mod Builder

Geode Mod Builder is an in-game visual editor for building simple Geode mods
without writing C++ by hand.

## What it does

- Create and manage projects (metadata, screens, elements, actions, assets).
- Lay out UI elements on a canvas: buttons, labels, sprites, popups, panels,
  screens, checkboxes, sliders, text inputs and close buttons.
- Configure properties per element type and wire up actions and events.
- Preview the resulting layout inside the editor (this is a preview, not a
  live copy of the Geometry Dash interface).
- Validate the project and generate a real Geode C++ project (`mod.json`,
  `CMakeLists.txt`, `src/*.cpp/.hpp`, `resources/`, `about.md`).
- Build the generated project if a compatible toolchain is available.

## Where projects are stored

Projects live in the mod's save directory, as structured JSON with a schema
version. Nothing is executed from project data; all paths are validated before
files are written.
