# Dual output (0.6): 16:9 and 9:16 from one composition

Goal: the operator builds the show once, in 16:9. Layouts keeps a vertical 9:16 twin of
every broadcast scene up to date (TikTok, Reels, Shorts), with the **same sources**, and
a CUT in the Régie switches both at once.

Status: design + pure core (`src/core/portrait-map.*`, unit-tested) + an OBS-side
prototype behind the `dual.enabled` flag (off by default).

## 1. What OBS 33 gives us

Canvases arrived in libobs 31 and are exposed to plugins in OBS 33
(`.deps/obs-studio-33.0.1/libobs/obs.h`, `frontend/api/obs-frontend-api.h`):

| API | Use |
| --- | --- |
| `obs_frontend_add_canvas(name, ovi, flags)` / `obs_frontend_remove_canvas` | A canvas saved with the scene collection, listed by the frontend. |
| `obs_frontend_get_canvases(&list)` | Every additional canvas (the main one is `obs_get_main_canvas()`). |
| `obs_canvas_get_video_info`, `obs_canvas_reset_video` | Base size of the canvas (1080x1920 for vertical). |
| `obs_canvas_scene_create(canvas, name)`, `obs_canvas_enum_scenes`, `obs_canvas_get_scene_by_name` | Scenes owned by a canvas. |
| `obs_canvas_set_channel(canvas, 0, scene)` / `obs_canvas_get_channel` | What the canvas puts on air (its "program"). |
| `obs_canvas_get_video(canvas)` | The `video_t` an encoder can take: `obs_encoder_set_video(enc, video)`. |
| `OBS_FRONTEND_EVENT_CANVAS_ADDED / _REMOVED` | Follow canvases created by other plugins. |
| signal `canvas_video_reset` (global handler) | Canvas resized; Layouts already listens (`constraints.cpp`). |

Flags: `PROGRAM = ACTIVATE | MIX_AUDIO | SCENE_REF`. Sources are shared by reference
across canvases (a scene item of the vertical scene can point to the camera that the 16:9
scene shows): one capture, one decoder, two placements.

Layouts already copes with several canvases: `EditContext` reads the size of the canvas
of the edited scene, `builder::create` and `commands::newScene` create scenes in that
canvas, `aliveSceneUuids` and the Scenes list enumerate every canvas, and the selftest
`testMultiCanvas` creates a 1080x1920 "Layouts Vertical" canvas and checks that broadcast
layouts stack there.

## 2. Options

| Option | Pros | Cons |
| --- | --- | --- |
| A. Second canvas, twin scenes (chosen) | Native OBS 33, real 1080x1920 render, same sources by reference, other plugins see the scenes, encodable. | Needs someone to encode/stream the canvas (see 5). |
| B. One 16:9 canvas, a vertical crop (9:16 band of the 16:9 picture) | Trivial, no second render. | 607x1080 band upscaled, no real re-layout: the point of the feature is lost. |
| C. Private canvas + our own outputs | Full control. | Re-implements Aitum Vertical (settings, stream keys, recording); invisible to other tools. |
| D. Nested scenes (a vertical scene containing the 16:9 scene cropped) | No canvas API. | Same flaw as B. |

## 3. Architecture

```
16:9 scene "Duo"  (main canvas)              9:16 twin "Duo ▯"  (vertical canvas)
  template key Cam2Split                       layout key Cam2Split (Legacy)
  slot 0 cam  ── Cam A ───────── same source ──── slot 0 cam (top)
  slot 1 cam  ── Cam B ───────── same source ──── slot 1 cam (bottom)
  background, graphics  ─────── same sources ──── background, graphics (scaled to fit)
```

1. **Vertical canvas.** At enable time Layouts looks for one, in this order:
   1. the canvas recorded in the collection (`dual.canvas` uuid);
   2. a frontend canvas whose base is portrait and whose name contains "vertical"
      (Aitum Vertical, Layouts' own, anything the user made);
   3. any frontend canvas whose base is portrait;
   4. otherwise it creates "Layouts Vertical" 1080x1920 with `PROGRAM` flags.
   It never resizes or removes a canvas it did not create.
2. **Twin scene.** For each 16:9 broadcast scene, a scene of the same name plus " ▯" is
   created in the vertical canvas (`obs_canvas_scene_create`). It holds one scene item per
   filled slot, pointing to the same `obs_source_t` as the 16:9 item (shared by
   reference, never duplicated), plus the background and graphics.
3. **Layout of the twin** comes from `core::mapToPortrait` (pure):
   * the 23 legacy templates draw themselves in portrait (`buildBroadcast` on the
     portrait canvas: guests stacked, screens 16:9 across the width on top, PiP in the
     lower corner) — strategy `Legacy`, layout key = the 16:9 key;
   * landscape-only templates (0.5 inventive / chat / custom layouts) are generated from
     the slot counts — strategy `Generated`, key `Portrait/<c>c<s>s<h>h`: screens on top
     at full width (two columns from three screens, capped at half the height when
     guests remain), guests below (one per row when that keeps cells near square, else
     a grid; the host gets a full-width row from three guests), chat at the bottom
     (20 % panel, 8 % ticker);
   * each 16:9 slot is linked to a twin slot of the same kind (`twinOf`): the i-th camera
     goes to the i-th portrait camera, so the operator's order and the host survive.
   Cameras are cropped to cover their cell (`coverCell`, faces kept), screens fit.
   Round bubbles carry over; diagonal masks and tilts do not (vertical stays clean).
4. **Refresh.** The twin is rebuilt when the 16:9 scene gets a new template, a swap, a
   source change in a slot, or when the vertical canvas is resized
   (`canvas_video_reset`). Manual edits on the twin are kept until the next rebuild of
   that twin; a per-twin "detached" flag stops automatic rebuilds.

### Data model (collection, under the existing `layouts` key)

```json
"dual": {
  "enabled": false,
  "canvas": "<vertical canvas uuid>",
  "ownCanvas": true,
  "twins": [
    {
      "scene": "<16:9 scene uuid>",
      "twin": "<9:16 scene uuid>",
      "key": "Cam2Split",          // 16:9 template key at last build
      "portraitKey": "Cam2Split",  // or "Portrait/2c1s0h"
      "detached": false,
      "slots": [ { "source": "<source uuid>", "twinItem": 12 } ]
    }
  ]
}
```

Scenes are referenced by uuid (names change). The source is the same object in both
scenes, so renaming or changing its settings applies to both outputs automatically.

## 4. Régie and Layouts window

* **Régie.** A "Dual" toggle in the header. When on, every scene button shows a small
  vertical thumbnail next to the 16:9 one. **CUT applies to both**: the 16:9 scene goes
  to the main program (`obs_frontend_set_current_scene`) and its twin to the vertical
  canvas (`obs_canvas_set_channel(vertical, 0, twin)`) in the same call; a transition on
  the vertical canvas follows the main transition when one is available. A lock icon
  lets the vertical output stay on a scene (e.g. a vertical-only "Just chatting") while
  the 16:9 one moves on.
* **Layouts window.** The canvas toolbar gets a 16:9 | 9:16 segmented switch that
  opens the twin of the edited scene (EditContext pins it, all tools work there because
  they already read the canvas size). The broadcast gallery shows the portrait preview
  of each template under the landscape one, and a "Detach vertical" item in the scene
  menu stops automatic rebuilds of that twin.
* **Builder.** "Also build the vertical scenes" checkbox (on when dual is enabled).

## 5. Limits

* **Who outputs the vertical canvas.** OBS 33 streams and records the main canvas only.
  A canvas is encodable (`obs_canvas_get_video` → `obs_encoder_set_video`), but stream
  settings, keys and recording paths are a whole product. 0.6 does not output: when
  Aitum Vertical (or another plugin) owns the portrait canvas, Layouts builds the twins
  in it and that plugin streams/records. With Layouts' own canvas, the user can
  record it with a canvas-aware plugin, or we add a minimal "Record vertical"
  (`ffmpeg_muxer` on the canvas video, same encoder settings as main) in 0.7.
* **Aitum Vertical** is detected by a portrait frontend canvas named like "vertical".
  If a version keeps its canvas private (`obs_canvas_create_private`), it is not
  listed; Layouts then creates its own canvas and both coexist (to verify against the
  current Aitum release).
* **GPU.** A second canvas renders every frame: about one extra 1080x1920 composite.
* **Audio.** Sources are shared, so their audio is mixed once; the vertical canvas
  should not add its own copies of mic sources.
* **Graphics** made for 16:9 (lower thirds) are scaled to fit the width on the twin;
  dedicated vertical graphics are a later step (a "Vertical only" role).
* **Transitions** per canvas are not exposed by the frontend API in OBS 33; the twin
  CUT is a straight cut until the frontend offers more.

## 6. Plan

| Step | State |
| --- | --- |
| Portrait map (pure, tests) | done: `src/core/portrait-map.*`, `tests/test-portrait-map.cpp` |
| OBS prototype: find/create canvas, build/refresh the twin of a scene, behind `dual.enabled` | prototype: `src/obs/dual.*`, called by the Builder when enabled; twins found by name (" ▯" suffix), the uuid `dual.twins` record is still to do |
| Régie CUT both, Layouts 16:9/9:16 switch, Builder checkbox | to do (UI) |
| Selftest: build twin of a Duo, check stacked cells, canvas and CUT | written (`testDualTwin`), not run yet |
| Record vertical (own canvas) | 0.7 |
