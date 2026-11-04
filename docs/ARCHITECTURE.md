# Architecture

## Overview

Layouts is a native OBS module (C++17, Qt 6, libobs, obs-frontend-api). Since 0.2 both parts live in
one regular window, **Layouts** (`ui/layouts-window`), opened from Tools > Layouts:

- **Inspector**: a properties panel for the selection (position, size, rotation, crop, bounding
  box, alignment, distribution, spacing).
- **Layouts Canvas**: an editing surface with its own `obs_display`, rulers, guides, layout grids,
  smart snapping and measurements.

The native OBS preview is never modified. Both docks act on the real OBS scene items and share
OBS's selection, so the Sources dock, the native preview and Layouts always agree.

```
src/
├── core/            pure C++17, no Qt, no libobs — unit tested
│   ├── geometry       Vec2, Rect, Affine, Quad, reference points
│   ├── item-model     faithful model of the OBS item transform + editing operations
│   ├── arrange        align, distribute, exact spacing
│   ├── snapping       smart snapping (tiers), equal spacing, measurements
│   ├── grid           square / column / row grids, ruler graduation
│   ├── expression     math input parser (no code execution)
│   └── guides         guide document, persistence records, schema migration
├── obs/             the only layer that calls libobs / obs-frontend-api
│   ├── edit-context   edited scene, item snapshot, selection, live state
│   ├── edit-session   atomic transactions + OBS undo/redo
│   ├── commands       editing commands shared by every UI
│   ├── store          guides (scene collection) and view settings (plugin config)
│   └── obs-convert    libobs <-> core conversions
├── ui/              Qt widgets
│   ├── theme          design tokens, icons, localized text
│   ├── widgets        numeric field with math + scrub label, reference point picker
│   ├── inspector-dock
│   ├── layouts-window the top-level window: Canvas + Inspector in a splitter
│   ├── canvas-dock    toolbar, scene picker, rulers, status bar, settings dialog
│   ├── canvas-view    obs_display surface, rendering, interactions
│   └── ruler
├── plugin-main.cpp  module entry, docks, frontend events, save callback
└── selftest.cpp     in-OBS self-test (development builds only)
```

## Data flow

```
libobs signals (any thread) ──► EditContext::markDirty() ──queued──► itemsChanged()
                                                                       │
              ┌─────────────────────────────── UI thread ──────────────┤
              ▼                                                         ▼
     EditContext::items()  (snapshot rebuilt lazily)        Inspector / Canvas / rulers refresh
              │
  user action ┴► commands::*  ──► core engines ──► EditSession (atomic apply + one undo entry)
                                                         │
                                                         ▼
                                                  OBS scene items
```

- **Snapshot.** `EditContext` keeps an immutable list of `ItemEntry` (OBS refs, transform model,
  world-space visual frame, flags). It is replaced, never mutated, and the previous lists stay
  alive until the event loop runs again, so pointers obtained during a call are always valid.
- **Commands** never read libobs matrices after a write; geometry comes from `core`.
- **EditSession** wraps a change: `begin()` saves the transform states, `apply()` writes inside
  `obs_scene_atomic_update()` (callable many times during a drag), `commit()` records one undo
  entry, `cancel()` restores the start state (Escape).

## Canvas rendering

The draw callback runs on the graphics thread:

1. Viewport = the whole display, orthographic projection = the visible canvas region, so zoom and
   pan never create huge viewports.
2. Black backdrop, then the scene: OBS's main texture when the edited scene is the program scene
   of the main canvas without Studio Mode, otherwise `obs_source_video_render(scene)`.
3. The area outside the canvas is masked.
4. Overlays in device pixels from an immutable snapshot built on the UI thread: grid, safe
   areas, outlines, selection, guides, snapping feedback, measurements, rubber band, handles,
   live frame.
5. Text labels are rendered by Qt into images on the UI thread and uploaded as textures
   (cached, evicted when unused).

The display is disabled when the dock is hidden. The edited scene is marked as "showing"
(`obs_source_inc_showing`) while the Canvas displays it, like a projector.

## Persistence

| Data | Where | Scope |
|---|---|---|
| Scene guides | scene collection, `modules.layouts`, via the frontend save callback | per scene (UUID, scene name kept as fallback) |
| Global guides | same | every scene of the collection |
| View settings (rulers, grid, snapping, safe areas, reference point, ratio lock, live policy, guide color) | `plugin_config/obs-layouts/settings.json` | global |

Collection document (`modules.layouts`, schema version 1):

```json
"layouts": {
  "version": 1,
  "scenes": [ { "uuid": "…", "name": "Interview", "guides": [ { "axis": "x", "pos": 960, "locked": false } ] } ],
  "global": [ { "axis": "y", "pos": 540, "locked": true } ]
}
```

- Unknown or invalid entries (bad axis, NaN, |pos| > 100000) are dropped and logged.
- Version 0 (development builds, axis "v"/"h") is migrated on load. Newer versions are read best
  effort (axis and position are stable fields by design).
- Guides of scenes that no longer exist are dropped when the collection is saved. Guides saved
  without a UUID are re-attached by scene name.
- Settings are written with `obs_data_save_json_safe` (temp file + backup).

## Error handling

- Every command is a no-op when there is no scene, nothing selected, or the source has no size.
- Invalid numeric input is refused in the field (red border, reason in the tooltip) and never
  reaches OBS.
- Crops never remove the whole source; sizes are clamped to at least 1 px.
- On scene collection change the edited scene is released and the guide document reset; on exit
  every OBS reference held by the plugin is released before libobs shuts down.

## Design decisions

1. Hybrid architecture, public APIs only (see [FEASIBILITY.md](FEASIBILITY.md)).
2. OBS 33+ only; scenes of every OBS 33 canvas can be edited.
3. Live policy: banner by default, optional blocking of edits on air.
4. W/H are the visible size, like Figma.
5. Guides per scene plus global guides.
6. No opacity control.
7. Shortcuts follow OBS conventions where they overlap with Figma's.
