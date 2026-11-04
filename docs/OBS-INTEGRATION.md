# OBS integration

Layouts targets **OBS Studio 33.0.1 and later** and uses **public APIs only**: `libobs`,
`obs-frontend-api` and Qt. It never looks up internal OBS widgets (`findChild`), never installs
event filters on the OBS preview and never edits scene collection files.

Every API below was checked in the OBS 33.0.1 sources.

## APIs used

| Need | API |
|---|---|
| Docks | `obs_frontend_add_dock_by_id` |
| Scene followed | `obs_frontend_get_current_scene`, `obs_frontend_get_current_preview_scene`, `obs_frontend_preview_program_mode_active`, frontend events |
| Other canvases | `obs_frontend_get_canvases`, `obs_canvas_enum_scenes`, `obs_source_get_canvas`, `obs_get_main_canvas` |
| Read transforms | `obs_sceneitem_get_info2`, `obs_sceneitem_get_crop`, `obs_sceneitem_get_draw_transform` (groups), `obs_source_get_width/height` |
| Write transforms | `obs_scene_atomic_update` + `obs_sceneitem_set_info2` / `obs_sceneitem_set_crop` |
| Selection | `obs_sceneitem_selected`, `obs_sceneitem_select`, scene signals `item_select` / `item_deselect` |
| Change tracking | scene and group signals `item_add`, `item_remove`, `reorder`, `refresh`, `item_visible`, `item_transform`, `item_locked`, source `remove` |
| Undo / redo | `obs_scene_save_transform_states`, `obs_scene_load_transform_states`, `obs_frontend_add_undo_redo_action` |
| Persistence | `obs_frontend_add_save_callback`, `obs_module_config_path`, `obs_data_*_json_safe` |
| Canvas surface | `obs_display_create`, `obs_display_add_draw_callback`, `obs_display_set_enabled`, `obs_source_video_render`, `obs_render_main_texture_src_color_only`, `obs_source_inc/dec_showing`, `gs_*` |
| Live state | `obs_source_active`, `obs_frontend_streaming_active`, `obs_frontend_recording_active`, `obs_frontend_replay_buffer_active`, `obs_frontend_virtualcam_active` |
| Lock | `obs_sceneitem_set_locked`, `obs_sceneitem_locked` |
| Double-click | `obs_frontend_open_sceneitem_edit_transform` |

## Threads

- libobs signals arrive on any thread (UI or video thread). The handler only sets a dirty flag
  and posts one queued refresh to the UI thread.
- The Canvas draw callback runs on the graphics thread. It reads an immutable snapshot
  (`std::shared_ptr<const RenderSnapshot>`) swapped under a mutex; it never touches Qt widgets.
- Lock order follows the rule documented in `obs-scene.c`: graphics before scene, never the
  graphics mutex inside a scene mutex. The atomic update callback only calls scene item setters.

## Deferred updates in libobs

`obs_sceneitem_set_crop()` and every change to an item inside a group only flag the transform as
dirty; libobs recomputes the matrices on the next video tick. Layouts therefore never reads
libobs matrices right after writing: geometry is computed by the core from the stored values, and
the snapshot is rebuilt immediately after each change.

## Undo

Each operation records one entry in OBS's own *Edit > Undo* stack, with the same mechanism OBS
uses for its transform undo (transform states of every item of the scene, groups included, crop
and bounds included). A whole mouse drag is one entry. Keyboard nudges use the `repeatable` flag,
so consecutive nudges merge like OBS's.

## Live safety

| State | Meaning | Behavior |
|---|---|---|
| Off | the edited scene is not shown on the program output | normal |
| Program | `obs_source_active(scene)` and no output running | amber banner |
| On air | program + streaming, recording, replay buffer or virtual camera | red banner and red frame around the canvas; edits allowed by default, refused when *Block edits on a scene that is on air* is checked |

Nested scenes count: a scene used as a source in the program scene is active.

## Rendering cost

- When the edited scene is the program scene of the main canvas and Studio Mode is off, the
  Canvas draws OBS's main output texture: no extra scene render.
- Otherwise (Studio Mode, pinned scene, other canvas) it renders the scene again, like a
  projector.
- The display is disabled when the dock is hidden; nothing is rendered.

Measured numbers are in [FEASIBILITY.md](FEASIBILITY.md).

## What is not possible with public APIs

- Rulers, guides or overlays on the native OBS preview (no access to its display, zoom or pan).
- Snapping inside a drag in the native preview.
- A per-item opacity (OBS has no such property; a filter would change the source in every scene).

These are the reasons for the dedicated Layouts Canvas.
