# Feasibility report

Phase 0 of Layouts. Every claim below was checked in the OBS Studio 33.0.1 sources and, when
marked **measured**, verified in a running OBS 33.0.1 (macOS 27, Apple Silicon) with the in-OBS
self-test (`-DLAYOUTS_SELFTEST=ON`, see [TEST-PLAN.md](TEST-PLAN.md)).

## Summary

| Question | Answer |
|---|---|
| Inspector with the public API | **Yes** (measured) |
| Multi-selection, align, distribute | **Yes** (measured) |
| One OBS undo entry per operation / per drag | **Yes** (measured) |
| Rulers, guides, overlays on the native preview | **No** with public APIs |
| Same features on a dedicated surface (`obs_display`) | **Yes** (measured) |
| Overlays never in the video output | **Yes** (measured: 0 differing pixels) |
| Performance | **Yes**: ≤ 1.6 ms per Canvas frame at 250 items, 0 lagged frames (measured) |

Decision: **hybrid architecture**, public APIs only: Inspector dock + Layouts Canvas dock.

## The four prototypes

### P0-1 · Selection sync — validated
`obs_sceneitem_select()` / `obs_sceneitem_selected()` and the `item_select` / `item_deselect`
signals keep the Sources dock, the native preview and Layouts in sync in both directions.
Group children signal on the group's own scene, so Layouts subscribes to every group scene too.

### P0-2 · Undo with transform states — validated
`obs_scene_save_transform_states()` stores position, scale, rotation, alignment, bounds type,
bounds alignment, bounds, crop-to-bounds and crop, for every item including group children.
`obs_frontend_add_undo_redo_action()` adds the entry to OBS's own stack.
Measured: *Edit > Undo* restores an alignment, *Redo* re-applies it, a 10-step mouse drag is undone
in one step.

### P0-3 · Canvas, ruler, guide, drag with snapping — validated
An `obs_display` owned by Layouts renders the scene with `obs_source_video_render()` (or the main
texture), and all overlays are drawn in its draw callback.
Measured: a real mouse drag (CGEvent) of an element ends exactly on a guide (snapped from 3 px
away); dragging from the top ruler creates a guide; docking and undocking the Canvas keeps it
rendering (30 frames/s in both states).

### P0-4 · Cost — validated
Measured on a 1920×1080 / 30 fps canvas, Studio Mode on (the Canvas renders the scene again):

| Items | Canvas draw avg | Canvas draw max | OBS frame time avg | Lagged frames |
|---|---|---|---|---|
| 10 | 0.26 ms | 0.59 ms | 5.3 ms | 0 |
| 50 | 0.44 ms | 0.84 ms | 4.4 ms | 0 |
| 100 | 0.77 ms | 1.70 ms | 5.3 ms | 0 |
| 250 | 1.48 ms | 2.55 ms | 5.4 ms | 0 |
| any, Canvas hidden | 0 (no frame drawn) | — | — | 0 |

Color sources were used; heavy sources (browser, capture) cost what they cost in a projector.
When Studio Mode is off and the Canvas follows the program scene, it draws the existing output
texture instead of rendering the scene a second time.

## Geometry fidelity (measured)

Layouts computes every geometry itself (`core::computeGeometry`) and compares it with
`obs_sceneitem_get_box_transform()` / `obs_sceneitem_get_draw_transform()` on 10 transforms
(rotation, center/right/bottom alignment, flips, crop, every bounds type, crop to bounds):
**maximum error 0.00013 px**.

Finding: `obs_sceneitem_set_crop()` and changes to group children only flag the item; libobs
recomputes its matrices on the next video tick. Reading libobs matrices right after a write
returns stale values (54 px error in the first test run). Layouts therefore never reads matrices
after writing.

## Live safety (measured)

- Program scene, no output: state *Program*.
- Program scene while recording: state *On air*; with *Block edits* the nudge is refused and the
  item does not move; without it the edit applies.
- Studio Mode with scene duplication: the program shows a private copy, editing the original is
  not on air: state *Off*. This is correct and documented.

## Output cleanliness (measured)

With grid, guides, safe areas and center lines all visible on the edited program scene, an OBS
screenshot of the output (`obs_frontend_take_screenshot`) is compared with the scene's solid
color: **0 differing pixels** out of 129,600 sampled.

## What needs internal APIs (not done)

| Feature | Why | Alternative in Layouts |
|---|---|---|
| Rulers / guides on the native preview | no API to its display, zoom or pan | Layouts Canvas |
| Snapping during a drag in the native preview | `MoveItems` applies incremental deltas internally | Layouts Canvas |
| Per-item opacity | no such property in libobs | out of scope (decision 6) |

## Memory

OBS counts leaked allocations at exit. With Layouts loaded but doing nothing, a scripted quit
reports 1. Each scenario replayed with plain libobs calls (Layouts not involved) reports the same
count as the same scenario driven by Layouts: collection switches 2, adding/removing a canvas 4.
No measured leak is attributable to Layouts. Normal runs of OBS quit by hand report 0.
