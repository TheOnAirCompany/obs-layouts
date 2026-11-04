# Implementation plan

Status legend: ✅ done in 0.1.0 · ⏭ next.

## 0.1.0

| # | Task | Depends on | Acceptance | Status |
|---|---|---|---|---|
| 1 | Scaffold from the OBS 33.0.1 plugin template, CI on 3 OS | — | builds | ✅ |
| 2 | `core/geometry`, `core/item-model` faithful to libobs | — | unit tests, < 0.01 px vs libobs | ✅ |
| 3 | `core/arrange`, `core/snapping`, `core/grid`, `core/expression`, `core/guides` | 2 | unit tests | ✅ |
| 4 | `EditContext` (scene follow / pin, snapshot, selection, live state) | 2 | self-test | ✅ |
| 5 | `EditSession` (atomic apply, undo) | 4 | undo / redo self-test | ✅ |
| 6 | `commands` | 3, 5 | self-test | ✅ |
| 7 | Store: guides in the collection, settings file | 3 | collection switch self-test | ✅ |
| 8 | Inspector dock (single, multi, mixed, live banner) | 6 | manual + self-test | ✅ |
| 9 | Canvas display, rendering, overlays | 4 | output screenshot clean, perf | ✅ |
| 10 | Canvas interactions (select, move, resize, rotate, rubber band, pan, zoom, keyboard) | 9, 6 | drag + snap self-test, real clicks | ✅ |
| 11 | Rulers, guides from rulers, guide menu | 9, 7 | ruler self-test, real drag | ✅ |
| 12 | Grids, safe areas, measurements, smart guides | 9, 3 | manual | ✅ |
| 13 | Settings dialog, toolbar, status bar | 9 | manual | ✅ |
| 14 | Localization EN / FR | 8, 13 | both locales complete | ✅ |
| 15 | Docs, README, screenshots | all | — | ✅ |
| 16 | Release packages (pkg, zip, deb) via CI, draft release | all | CI green | see RELEASE-PLAN |

## 0.2 (next)

| Task | Notes |
|---|---|
| Auto layout (horizontal, vertical, grid; spacing, padding, alignment) applied on demand | ✅ on `main` (`core/layout`) |
| Constraints (left, right, center, left & right, scale) re-applied on canvas size change | ✅ on `main` (`canvas_video_reset` signal, item private settings) |
| Layout presets (webcam corners, picture in picture, split screen…) with a preview before applying | ✅ on `main` |
| Crop by dragging handles | ✅ on `main`: crop mode (C), since Alt already resizes from the center |
| Multi-selection resize on the Canvas | ✅ on `main` |
| Configurable shortcuts through OBS hotkeys | ✅ on `main` (`obs_hotkey_register_frontend`, bindings in `hotkeys.json`) |
| Broadcast layouts: 23 TV-style templates for 1–6 cameras and 0–2 screens, camera cover crop, screen fit, lock, new scene | ✅ on `main` (`core/broadcast`, Inspector section, self-tests `broadcast-*`) |
| Broadcast layouts on portrait canvases, lock checkbox decides the final state | ✅ on `dev/0.2.1` (PR #2) |
| Design review follow-up | ✅ every token approved (2026-11-04), gallery colors `kThumbCamera` / `kThumbScreen` |
| Windows and Linux runtime validation, signed packages | needs certificates |
