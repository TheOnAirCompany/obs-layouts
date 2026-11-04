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
| Auto layout (horizontal, vertical, grid; spacing, padding, alignment) applied on demand | uses `core/arrange`; stored in item private settings |
| Constraints (left, right, center, left & right, scale) re-applied on canvas size change | `video_reset` canvas signal |
| Layout presets (webcam corners, picture in picture, split screen…) with a preview before applying | never duplicates sources |
| Crop by dragging handles with Alt (OBS convention) | — |
| Multi-selection resize on the Canvas | scale the selection box |
| Configurable shortcuts through OBS hotkeys | — |
| Design review follow-up | tokens in `src/ui/theme.hpp` |
| Windows and Linux runtime validation, signed packages | needs certificates |
