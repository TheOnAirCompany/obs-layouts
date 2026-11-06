# Changelog

## 0.7.0 — 2026-11-07

- **Layouts Switcher set aside**: the Switcher dock, the lost camera failover, their hotkeys and the
  obs-websocket requests are not part of this version. Their code stays in the repository behind the
  `LAYOUTS_REGIE` build option (off by default); their saved settings are kept in the scene
  collection.
- **Two windows** instead of one: **Tools > Scene Editor** (FR *Éditeur de scènes*) to edit a scene
  (Scenes navigator and Layers, Canvas, Inspector) and **Tools > Scene Builder** (FR *Créateur de
  scènes*) to make the scenes of the show from the sources. Both can be open at once; each has a
  button that opens the other. The Scene | Builder switch is gone. The "Open the Layouts window"
  hotkey opens the Scene Editor; a new hotkey opens the Scene Builder.
- **Scenes navigator** above Layers, replacing the scene menu of the Canvas toolbar and the flat
  Scenes list: one section per canvas (Main 16:9, Vertical 9:16...), the stacks of Stacks for OBS
  inside when it is installed (colors, counts, collapsible), search, All / 16:9 / 9:16 / favorites
  filter, red dot on program, green dot in preview (a collapsed stack shows the dot of a scene it
  holds), "+" for a new scene in the canvas and stack of the edited scene.
- **Follow OBS** is a pin in the Canvas toolbar (blue when on); a click on a scene in the
  navigator turns it off. A **breadcrumb** names the edited scene: `16:9 › Plateau › Duo`.
- **On air, only when it is true**: the amber PROGRAM and red ON AIR banners are replaced by a
  red **● PROGRAM** chip in the Canvas toolbar and the red frame, shown only when the scene is on
  the program of its canvas and that canvas is streamed or sent to a virtual camera (OBS streaming
  or virtual camera for the main canvas; a streaming output or Vertical's virtual camera for the
  others). Recording alone no longer counts. "Block edits on air" follows the same rule.
- **Scene Builder** in three columns: sources of the collection (search, type chips), layouts for
  the selection with a **16:9 / 9:16 / Both** target and the mirror toggles, and Create: a large
  preview with the source names, the scene name, **Put in stack** (Stacks for OBS), BG at the back,
  CG on top, Create scene, Create show scenes… (with a confirmation) and **Open in Scene Editor ↗**.
  9:16 makes the scene in the canvas of Vertical for OBS; Both makes the 16:9 scene and its 9:16
  twin, linked in Vertical for OBS. The Builder stays open after Create and shows "Created: …".
- Uses the new API of Stacks for OBS (`stacks_get_organization`, `stacks_place_scene`, signal
  `stacks_changed`) when it is installed; works without it (flat lists, no "Put in stack").

## 0.6.0 — 2026-11-06

- **Signed and notarized macOS package** (Developer ID): macOS installs it without warning.
- **AUTO** in the Layouts Switcher: the program glides to the prepared layout inside the same
  scene, no scene transition: tiles move and resize, crops and rotations follow, tiles that leave
  fade out, new ones fade in. Duration next to the button (300 ms by default, 0 = cut). One undo
  step. Also from obs-websocket (`Auto`).
- **Vertical twin** (with Vertical for OBS): each Plateau gets a 9:16 twin built from the same
  sources (screens on top, guests stacked, chat at the bottom), and CUT / AUTO switch the vertical
  output at the same time ("Also switch the vertical 9:16 output" in the Switcher). Layouts never
  creates a canvas itself: it uses the one Vertical for OBS exposes, or an existing portrait canvas.
- Fixed: the lost camera failover, the Switcher tally and AUTO now act on the scene really on air
  (in Studio Mode with "duplicate scene", OBS shows a private copy of the scene).

## 0.5.0 — 2026-11-06

- **Signed and notarized macOS package** (Developer ID): macOS installs it without warning.
- **118 broadcast layouts for 16:9** (23 before): 33 more with cameras only, 42 more with screens
  (screens alone, screens + cameras) and 20 inventive ones, 10 of them with a chat: tilted cards,
  Polaroid, bento, dock of bubbles, diagonal duo, card stack, tilted screen, masonry wall,
  porthole... Layouts drawn for landscape only are hidden on a portrait canvas.
- **Chat**, a new source type (pill CHAT, light green), suggested from the start of the name
  (`CHAT`, `TCHAT`, `CHATBOX`). A chat is fitted whole in its tile, never cropped: at the top of a
  column, centered in a ticker.
- Slots can be **rotated**, **round** (radius 50 %), **cut on a diagonal** or **framed** (Polaroid
  border); the host goes in the slot made for the host. When slots overlap they are stacked back to
  front as the layout draws them; the background stays below and the graphics above.
- **Mirror ↔ ↕** toggles above the gallery: the layouts swap sides (left / right, top / bottom).
  Only the positions move; the image of a source is never flipped.
- **Style** section in the Inspector (it replaces the corner radius field of Dimensions):
  - Corners: Auto (4 % of the short side, between 6 and 48 px), px, Round, None.
  - Border: 4 px white when you turn it on, inside the frame, following the corners.
  - Shadow: None, Soft, Hard or Custom (blur, offset, opacity, color).
  - Fade during transitions, Move Transition included.
  - **Styles by type…**: a default style per type of source (radius Auto, no border, no shadow
    until you change it). An item with its own style shows a "modified" dot and a **Reset to type**
    button. Every edit is one Edit > Undo step.
- **Handles like a design tool**: the ratio lock decides. Locked, corners and edges keep the ratio
  (an edge grows around the middle of the other side); unlocked, everything is free. Shift
  inverts the lock during a drag, Alt resizes from the center, both at once without moving the
  mouse. A free resize follows the frame mode: Fill crops, Fit letterboxes, only Stretch distorts.
  A badge shows the ratio (16:9, 1:1...) while you drag.
- **Swap**: drag a camera onto another camera (a screen onto a screen, a chat onto a chat) and they
  exchange their slots. The place, rotation, stacking and style stay with the slot; flips and
  frame mode stay with the source. Works on locked layouts; one undo step. Hold Alt for a plain
  move.
- **Save as layout**: your own scene becomes a layout of the gallery, marked with a dot. Rename,
  delete, export; import and export JSON files to move them between machines.
- **Switcher dock** (Docks > Layouts Switcher): pick a Plateau scene and recompose it live like a
  vision mixer. A LAYOUTS row, one BUS row per slot, a PREVIEW line and CUT (AUTO comes in 0.6).
  Red = on air, green = in preview. With Studio Mode, changes are prepared in preview and go on
  air with CUT. OBS hotkeys (cut, next / previous layout, layouts 1 to 9, slot N = source M) and
  obs-websocket requests (vendor `layouts`: GetState, ListLayouts, SetLayout, AssignSlot, Swap,
  Take, event StateChanged) for Stream Deck and Companion.
- **Lost camera**: when a camera or a screen on air stops sending video for 1.5 s, the program
  drops to the same layout with one tile less, at once and without a transition, and the Switcher
  shows a red banner. When the source is back and stable for 2 s, the original layout returns by
  itself. On by default (checkbox in the Switcher).
- Fixed: the Windows build (float literals).
- Self-tests release the scenes they create while OBS still runs (no more crash on exit after
  the corners tests), and cover round, diagonal, border and shadow pixels, the Switcher CUT and the
  lost camera failover end to end.
- The drop shadow also shows on cropped items (cameras in Fill) in the program output (stream,
  recording, virtual camera): OBS draws them through a texture cut to their visible part, so their
  shadow is drawn in the scene just before them. Limit: a second view of the same scene in the same
  frame (projector or multiview of the program) reuses that texture and shows it without shadow.

## 0.4.1 — 2026-11-06

- Fixed: the Windows build (float literals) and the Linux build (GCC). Same features as 0.4.0.

## 0.4.0 — 2026-11-06

- **Source types**: Host, Cam, Screen, BG (background) and CG (graphics: logo, banner, name,
  frame). Set them from a colored pill on each row (several sources at once); they are saved
  with the scene collection and kept when a source is renamed. Until you set one, Layouts
  suggests it from the start of the name (`CAM _ GUEST 1`, `SCREEN _ RIGHT`, `BG _ DYNAMIQUE`,
  `LOGO`, `HOST`..., case and accents ignored), then from the kind of source (video capture,
  NDI, DeckLink = Cam; display or window capture = Screen). A web source named `CAM ...` is now
  a camera in broadcast layouts.
- **Filter Layers by type**: a row of chips (All, Host, Cam, Screen, BG, CG, Not set) with counts.
- **Frame modes: Fill, Fit, Stretch** (Inspector, Dimensions). Changing W and H, or a ratio button
  (16:9, 1:1, 4:5, 9:16), changes the shape of the frame without distorting the image: Fill (the
  default) covers the frame and crops what goes past, Fit keeps the whole image with empty bands,
  Stretch distorts as OBS does. The 3×3 reference point is both the point that stays in place and
  the part of the image that is kept. Rounded corners sit on the visible frame. A resize that
  keeps the shape only scales, so a manual crop is kept.
- **Scenes above Layers**, like the pages of a design tool: click a scene to edit it in the Layouts
  window without changing what is on air (the first row follows OBS); the scene on the program
  has a red dot. Handy to fix one element of a scene made by the Builder.
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
- Rounded corners stay during **Move Transition** (exeldro) and other transitions that draw the
  sources themselves: the radius goes smoothly from the first scene's to the second's.

## 0.3.1 — 2026-11-06

- Fixed: the Windows build (float literal). Same features as 0.3.0.

## 0.3.0 — 2026-11-06

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
