<div align="center">

# Layouts for OBS

[![Build](https://github.com/TheOnAirCompany/obs-layouts/actions/workflows/push.yaml/badge.svg?branch=main)](https://github.com/TheOnAirCompany/obs-layouts/actions/workflows/push.yaml) [![Core tests](https://github.com/TheOnAirCompany/obs-layouts/actions/workflows/core-tests.yaml/badge.svg?branch=main)](https://github.com/TheOnAirCompany/obs-layouts/actions/workflows/core-tests.yaml) [![Release](https://img.shields.io/github/v/release/TheOnAirCompany/obs-layouts?include_prereleases&sort=semver)](https://github.com/TheOnAirCompany/obs-layouts/releases) [![OBS Studio](https://img.shields.io/badge/OBS%20Studio-33.0.1%2B-302E31?logo=obsstudio&logoColor=white)](https://obsproject.com) [![Platforms](https://img.shields.io/badge/platforms-macOS%20%7C%20Windows%20%7C%20Linux-informational)](#install) [![Languages](https://img.shields.io/badge/languages-English%20%7C%20Fran%C3%A7ais-blue)](data/locale) [![License](https://img.shields.io/github/license/TheOnAirCompany/obs-layouts)](LICENSE)

**Design your scenes. Pixel perfectly.**

Layouts for OBS is a native OBS Studio plugin that brings the composition tools of a design app to your scenes: a permanent **Inspector**, **rulers**, **guides**, **layout grids**, **smart snapping**, **alignment**, **distribution** and **measurements** — without touching your video output.

</div>

![The Layouts window: Canvas with rulers and guides, Inspector on the right](docs/images/layouts-window.png)

> **Status: 0.2.0 preview.** Tested in OBS Studio 33.0.1 on macOS (Apple Silicon). Windows and Linux packages are built by CI but not tested at runtime yet. Packages are unsigned. Keep a backup of your scene collection and report anything odd in [Issues](https://github.com/TheOnAirCompany/obs-layouts/issues).

## What it adds

### Layers

The left panel of the Layouts window: every source of the edited scene, front on top, groups unfolded. Click to select (Shift / Cmd for several), the eye hides, the lock locks, double-click renames, drag reorders (undoable). No need to go back to the OBS Sources dock.

### Inspector

The right panel of the Layouts window. It always shows the selection of the edited scene.

- **Position, size, rotation** in canvas pixels, from a **reference point** you pick on a 3×3 grid. W and H are the size you see, after scale, crop and bounding box.
- **Math in every field**: `1920/2`, `100% - 120`, `+20`, `-10`, `*1.5`, `=-10` for an absolute negative value. Drag a field label to scrub (Shift ×10, Alt ×0.1).
- **Ratio lock**, **Fit / Fill / Stretch** to the canvas, **original size**, **reset**, **rotate ±90°**, **flip**.
- **Crop** in source pixels (the image stays in place), **bounding box** type and alignment.
- **Multiple selection**: align to the selection, the canvas or a **key object**; distribute centers or spacing; type an **exact gap**; same width / height / size; **Mixed** values.
- **Copy / paste properties** (position, size, rotation, crop, bounding box).
- **Broadcast layouts**: select 1 to 6 cameras and up to 2 screens, pick a TV-style layout (duo, three columns, host + guests, grids, screen + guests on the side or below, picture in picture) and apply it, or make a new scene with it. Cameras fill their tile, screens are never cropped. Works on vertical canvases too (guests stack, screens on top). **Create show scenes** makes, in one click, a full-screen scene per guest and screen plus a scene for every layout that fits. **Lock the layout** keeps everything in place.

  ![Broadcast layouts in the Inspector, three columns previewed on the Canvas](docs/images/broadcast-layouts.png)

- **Auto layout**: rows, columns or a grid, with gap, padding and alignment, inside the selection or the whole canvas, previewed on the Canvas before you apply it.
- **Presets**: corners, centered, full screen, lower third area, picture in picture, split screen, one large + two small, 2 × 2 grid, with margins you set.
- **Constraints** (Advanced): pin elements to an edge, the center, both edges or scale them, so they keep their place when the canvas resolution changes.

### Layouts Canvas

The editing surface in the middle of the Layouts window, next to the OBS preview.

- **Rulers** in canvas pixels that follow zoom and pan, with the selection highlighted.
- **Guides**: drag them out of the rulers, move, lock, type an exact position, delete by dragging back. Per scene, or **global** for every scene. Saved with the scene collection.
- **Layout grids**: square grid (presets 8 to 64 px), columns, rows.
- **Smart snapping**: guides, grid, other elements (edges, centers, **equal spacing**), canvas — with red smart guides. Hold Ctrl/Cmd to bypass it, as in OBS.
- **Measurements**: hold Alt to see the distances to the hovered element or to the canvas.
- **Direct manipulation**: select, Shift-select, rubber band, move, resize from 8 handles (Alt from the center), rotate just outside a corner, nudge with the arrows. With several elements selected, the handles scale the whole selection.
- **Crop mode** (C): the handles crop the source and the image stays in place.
- **Zoom** (fit, 100 %, selection, pinch, Ctrl/Cmd + wheel) and **pan** (Space + drag, wheel).
- **Safe areas**, center lines and custom margins.
- Edit the scene OBS shows, or **pin any scene**, including scenes of other OBS 33 canvases (vertical...).

### Made for live shows

- An amber **PROGRAM** banner when you edit the scene on the program output, a red **ON AIR** banner and frame while streaming, recording, replay buffer or virtual camera run. Optionally **block edits on air**.
- Every change is applied **atomically** (never half an alignment on air) and is **one entry in OBS's Edit > Undo**, a whole drag included.
- Rulers, guides, grids and handles are drawn on Layouts' own display: they **never reach the video output** (verified pixel by pixel).
- Public OBS APIs only. The native preview, your sources and your scene collection files are never modified behind your back.

## Install

Download the file for your system from the [releases](https://github.com/TheOnAirCompany/obs-layouts/releases). OBS Studio **33.0.1 or later** is required.

| System | File | How to install |
|---|---|---|
| macOS 13+ (Apple Silicon and Intel) | `obs-layouts-<version>-macos-universal.pkg` | Open the package and follow the installer (it installs for the current user). The package is not notarized yet: if macOS blocks it, right-click it and choose *Open*, or allow it in *System Settings > Privacy & Security*. |
| Windows 10/11 x64 | `obs-layouts-<version>-windows-x64.zip` | Extract the `obs-layouts` folder into `C:\ProgramData\obs-studio\plugins\`. |
| Ubuntu x86_64 | `obs-layouts-<version>-x86_64-linux-gnu.deb` | `sudo apt install ./obs-layouts-<version>-x86_64-linux-gnu.deb`. Needs the `obs-studio` package **33 or later** installed system-wide. |

Restart OBS, then open **Tools > Layouts**. Layouts is a regular window (resize it, maximize it, put it in full screen) with the Canvas in the middle and the Inspector on the right. It never adds anything to the OBS window. You can also give it a key in **Settings > Hotkeys** ("Layouts: Open the Layouts window").

To uninstall, remove the plugin. Your scenes and sources are never touched; Layouts only stores its guides in its own section of the scene collection.

## Shortcuts (Canvas focused)

| Keys | Action |
|---|---|
| Arrows, Shift + arrows | move 1 px, 10 px |
| Shift + drag | constrain to an axis; on a corner handle: free ratio |
| Ctrl/Cmd + drag | no snapping |
| Alt | measure; Alt + resize: from the center |
| Space + drag | pan |
| Shift + 0 / 1 / 2 | zoom 100 % / fit / selection |
| + / − | zoom in / out |
| Shift + R, Shift + G, Ctrl/Cmd + ; | rulers, grid, guides |
| C | crop mode for the selected element |
| Esc | cancel a drag, leave crop mode, clear the selection |

Rulers, guides, grid, safe areas, snapping, the six alignments, distribution and zoom to fit can also get global keys in OBS **Settings > Hotkeys** (entries starting with "Layouts:").

## Documentation

| Document | Content |
|---|---|
| [Feasibility](docs/FEASIBILITY.md) | What OBS 33 allows, prototypes, measured performance |
| [Product requirements](docs/PRD.md) | Vision, personas, user stories, scope |
| [UX specification](docs/UX-SPECIFICATION.md) | Every interaction, state and shortcut |
| [Design system](docs/DESIGN-SYSTEM.md) | Tokens and components |
| [Coordinate system](docs/COORDINATE-SYSTEM.md) | The math behind every number |
| [Architecture](docs/ARCHITECTURE.md) | Components, data flow, persistence |
| [OBS integration](docs/OBS-INTEGRATION.md) | APIs used, threads, undo, live safety |
| [Implementation plan](docs/IMPLEMENTATION-PLAN.md) | Done and next |
| [Test plan](docs/TEST-PLAN.md) | Unit, in-OBS and manual tests, results |
| [Release plan](docs/RELEASE-PLAN.md) | Packaging, compatibility, checklist |

## Building

The build system comes from [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate), updated for OBS Studio 33.0.1.

```sh
cmake --preset macos          # or windows-x64, ubuntu-x86_64
cmake --build --preset macos
```

Core unit tests (no OBS needed):

```sh
cmake -S tests -B build_tests && cmake --build build_tests && ctest --test-dir build_tests
```

In-OBS self-test (development only): configure with `-DLAYOUTS_SELFTEST=ON` and start a **disposable** OBS configuration with `LAYOUTS_SELFTEST=1`. See the [test plan](docs/TEST-PLAN.md).

## License

[GPL-2.0-or-later](LICENSE), like OBS Studio.

Icons: [Lucide](https://lucide.dev) (ISC).

© 2026 The OnAir Company
