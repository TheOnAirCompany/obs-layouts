# Changelog

## 0.4.0 — not released yet

- **Source types**: Host, Cam, Screen, BG (background) and CG (graphics: logo, banner, name,
  frame). Set them from a colored pill on each row (several sources at once); they are saved
  with the scene collection and kept when a source is renamed. Until you set one, Layouts
  suggests it from the start of the name (`CAM _ GUEST 1`, `SCREEN _ RIGHT`, `BG _ DYNAMIQUE`,
  `LOGO`, `HOST`..., case and accents ignored), then from the kind of source (video capture,
  NDI, DeckLink = Cam; display or window capture = Screen). A web source named `CAM ...` is now
  a camera in broadcast layouts.
- **Filter Layers by type**: a row of chips (All, Host, Cam, Screen, BG, CG, Not set) with counts.
- **Layouts Builder** (Tools > Layouts Builder, or the Scene | Builder switch at the top left of
  the window): every source of the collection once, whatever scene it is in, with its type and
  the number of scenes that use it. Select the sources you want (Cmd / Shift): the broadcast
  layouts gallery of Scene mode shows the layouts that fit them; New scene (or a double-click)
  makes the chosen one, with the background at the very back covering the canvas, the hosts in
  the large tile and the graphics on top at the place they already have. Create show scenes
  makes a full-screen scene per source and one per layout that fits.
- Broadcast layouts in Scene mode: hosts take the large tile; backgrounds and graphics in the
  selection are never put in a tile.
- **Rounded corners belong to the scene**: the same camera can be round in one scene and square
  in another. Corners set with 0.3 (for every scene) are split per scene the first time you edit
  one, so the other scenes keep what they had.

## 0.3.0 — not released yet

- **Layers panel** on the left of the Layouts window, like a design tool: the sources of the edited
  scene as a tree (front on top, groups unfold), selection in sync with the Canvas and OBS, eye and
  lock on each row, double-click to rename, drag to reorder or to move a source into or out of a
  group (the image stays in place). Hiding, locking, renaming, reordering and grouping are all in
  Edit > Undo.
- **Rounded corners**: a corner radius field in the Inspector (Dimensions), in canvas pixels. The
  corners are a clean anti-aliased mask that follows the crop, so a cropped camera in a broadcast
  layout keeps its four round corners. No shader plugin needed: Layouts adds its own "Rounded
  corners" filter to the source (also available in the OBS Filters window). The radius belongs to
  the source, so the same camera placed at another size in another scene gets proportional
  corners. Undoable.
- Broadcast layouts and show scenes keep a source's **flip**: a mirrored webcam or a camera
  mounted upside down stays that way in its tile instead of being flipped back.

## 0.2.0 — 2026-11-04

- **Layouts window**: the Canvas and the Inspector now live together in one regular window
  (resizable, maximizable, full screen), opened from Tools > Layouts or a hotkey. Nothing is added
  to the OBS window any more and nothing opens by itself at startup. Replaces the two docks of 0.1.
- Guides are undoable: adding, moving, locking, deleting or clearing guides is one entry in Edit > Undo
  (a whole drag included); Escape during a drag puts the guide back.
- **Broadcast layouts**: select your guests and screens, pick a TV-style layout and apply it, or
  create a new scene with it. 23 layouts for 1 to 6 cameras, with or without one or two screens:
  full screen, duo, three columns, host + 2, 2 × 2, host + 3, 2 + 3, host + 4, 3 × 2, screen +
  picture in picture, screen + guests on the side or below, two screens. Cameras fill their tile
  (cropped, keeping the top of the frame where faces are); screens are always shown whole. Cameras
  are detected (video capture devices, NDI, DeckLink, AJA), or count everything as cameras or as
  screens. Gap and margin settings, previewed on the Canvas. On a portrait canvas (9:16 for
  TikTok, Reels, Shorts) guests stack instead of sitting side by side and screens stay 16:9 across
  the top; the gallery thumbnails follow the canvas shape.
- **Create show scenes**: one click makes a full-screen scene for each selected source, then a
  scene for every broadcast layout that fits the selection (locked if the box is checked).
- **Lock the layout** (on by default): applied sources are locked so they can't be dragged by
  accident, and scale with the canvas if its resolution changes. Re-applying a layout with the box unchecked frees them
  again.
- Constraints moved into a collapsed "Advanced" part of the Inspector.
- Fixed: a crash when the cursor rested over the Inspector while it refreshed.
- Fixed: a violet preview frame could stay on the Canvas after the selection changed.

- Resize several selected elements at once from the selection handles on the Canvas (corners keep the
  ratio, Shift frees it, Alt resizes from the center, edges snap).
- Auto layout in the Inspector: horizontal, vertical or grid; gap, padding, alignment on both axes,
  keep sizes or fill with equal sizes; inside the selection or the whole canvas; applied on demand.
- Composition presets: corners, centered, full screen, lower third area, picture in picture, split
  screen, stacked, one large + two small, 2 × 2 grid; margin, gap and corner size settings.
- Ghost preview on the Canvas while hovering the auto layout or presets section, before applying.
- Crop mode on the Canvas (C or the context menu): the handles crop the source and the image stays
  in place. Items without a bounding box.
- OBS hotkeys for Layouts actions in Settings > Hotkeys (rulers, guides, grid, safe areas, snapping,
  the six alignments, distribute, zoom to fit); no key bound by default.
- Constraints (left, right, center, left and right, scale / top, bottom, center, top and bottom, scale)
  re-applied to every scene of a canvas when its resolution changes; saved with each item.

## 0.1.0 — preview

First version.

### Inspector
- Position, size, rotation from a 3×3 reference point; W/H as the visible size.
- Math expressions, relative values, percentages, scrubbing, mixed values.
- Ratio lock, fit / fill / stretch, original size, reset, rotate ±90°, flip.
- Crop (content stays in place) and bounding box settings.
- Align to selection, canvas or key object; distribute centers and spacing; exact gaps; same size.
- Copy / paste properties.

### Layouts Canvas
- Own `obs_display` rendering of the edited scene, follow OBS or pin any scene of any canvas.
- Rulers, guides (per scene and global, saved in the collection), square / column / row grids.
- Smart snapping with priorities and equal spacing, smart guides, Alt measurements.
- Selection, rubber band, move, resize, rotate, nudge, zoom and pan.
- Safe areas, center lines, custom margins.

### Safety
- Program / on-air banners, optional blocking of edits on air.
- Atomic changes, one OBS undo entry per operation and per drag.

### Quality
- 60 core unit tests; in-OBS self-test (geometry fidelity vs libobs, commands, undo, persistence,
  clean output, live states, multi-canvas, docking, performance up to 250 items).
- English and French.
