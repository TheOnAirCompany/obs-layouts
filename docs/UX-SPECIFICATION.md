# UX specification

Layouts brings design-tool composition to OBS while keeping OBS conventions where they already
exist. This document describes the behavior of version 0.1.0.

## The Layouts window (0.2)

One regular top-level window titled **Layouts**: the Canvas in the middle, the Inspector in a
resizable panel on the right. It can be resized, maximized and put in full screen; double-clicking
its title bar maximizes it like any macOS window and never docks it into OBS. It is opened from
**Tools > Layouts** or the "Layouts: Open the Layouts window" hotkey, never by itself at startup.
Closing it only hides it; its size, position and panel width are remembered. Nothing is added to
the OBS main window.

(0.1 used two OBS docks instead: an Inspector docked on the right and a floating Canvas.)

## Edited scene

The Canvas toolbar has a scene picker:

- **Follow OBS (scene name)** (default): the preview scene in Studio Mode, otherwise the program
  scene. Changes when OBS changes scene.
- **Any scene**, including scenes of other OBS 33 canvases (shown as `Canvas › Scene`): pinned
  until *Follow OBS* is chosen again or the scene is deleted.

The Inspector always shows the selection of the edited scene.

## Layers (0.3)
Left panel of the Layouts window (default width 240 px, resizable down to 160 px). The sources of the
edited scene as a tree, front on top like a design tool's layers; groups unfold and remember being
folded while the scene stays the same. Each row: type icon (camera, screen, window, image, text,
browser, media, color, group, scene, audio), name, lock, eye. Hidden sources are dimmed.
- Click selects, Shift / Cmd extends; the selection is the OBS selection, so the Canvas, the Inspector
  and the OBS Sources dock follow, and the other way round.
- The eye and the lock act on one source; both respect the block-on-air policy. Undoable.
- Double-click (or Return) renames the source; a name already taken is refused. Undoable.
- Drag and drop reorders, and moves a source into or out of a group (drop on the group or among its
  items); the image stays in place, as with OBS's own list (`obs_scene_reorder_items2`). A group
  can't go into a group (OBS has no nested groups). One row at a time. One undo entry, holding the
  whole order of the scene: libobs transform states do not carry the order or the grouping.

### Types and filter (0.4)
- A pill after the name shows the source's type: HOST, CAM, SCREEN, BG, CG, or a dashed "?" when
  not set. Clicking it opens the type menu; when the row is selected, the type applies to every
  selected source. One undo entry. Groups have no type.
- The type is saved in the source's private settings (scene collection), so renaming keeps it.
  Until set, it is suggested from the name prefix (Host: HOST, PRÉSENTATEUR, PRESENTER, ANIMATEUR;
  Cam: CAM, CAMERA, CAMÉRA, GUEST, INVITÉ; Screen: SCREEN, ÉCRAN, SCR, DISPLAY, CAPTURE; BG: BG,
  BACKGROUND, FOND; CG: CG, GFX, OVERLAY, LOGO, LT, HABILLAGE), case and accents ignored, the
  prefix followed by the end of the name, a space, `_`, `-` or `.`; the longest prefix wins. Then
  from the kind of source (video capture, NDI, DeckLink, AJA = Cam; display, window, game capture
  = Screen).
- Filter row above the list: All, then one chip per type present, with counts. Groups stay while
  one of their sources matches. Drag and drop is off while a filter is active.

## Layouts Builder (0.4)
Tools > Layouts Builder, or the Scene | Builder switch at the top left of the window (Tools >
Layouts opens Scene mode; the hotkey keeps the current mode). The Canvas stays in the center.
- Left: "Sources of the collection", every video input once (scenes and groups excluded), with an
  include mark (blue check = in the show; sources without a type can't be included), the source
  icon, the name, the number of scenes that use it, and the type pill (same menu as Layers). Same
  filter row.
- Right: "Generate": summary (1 Host · 2 Cam · 1 Screen · 1 BG · 1 CG), gap and margin (shared
  with broadcast layouts), options (a full-screen scene per host, cam and screen; every broadcast
  layout that fits; background in every scene; graphics in every scene; lock), the list of scene
  names, and "Create N scene(s)". The first new scene opens on the Canvas.
- Each scene: the first BG at the back, covering the canvas (bounding box "scale to outer bounds",
  centered, cropped to bounds); screens, then hosts and cams in their tiles (hosts first, so they
  get the large tile), flips kept; CG on top with the transform and crop they have in the first
  scene that uses them. Locked when "Lock" is checked. Names made unique ("Duo 2").

## Inspector

### Empty state
Scene name, canvas size and a hint on how to select.

### Single selection
| Section | Controls |
|---|---|
| Header | name, source type · scene › group, Lock, Copy properties, Paste properties ▾ |
| Align | 6 buttons: left, horizontal center, right, top, vertical center, bottom, relative to the canvas |
| Position | reference point picker (3×3), X, Y, rotation, rotate ±90°, flip H/V |
| Dimensions | W, H, ratio lock, Fit, Fill, Stretch, Original size, Reset transform, corner radius (0.3) |
| Crop | left, right, top, bottom (source pixels) |
| Bounding box | type, content alignment, crop to bounds |

#### Corner radius (0.3)
- One field (label ╭), in canvas pixels, 0 = square corners. Typing, math and scrubbing work as
  in every numeric field; with several items selected it shows Mixed when the radii differ. Groups
  have no radius.
- Applied with a "Rounded corners" filter that Layouts registers and adds last in the source's
  filter list (removed again at 0). The mask is anti-aliased and drawn on the visible part of the
  item: the crop of the scene item being drawn is read at render time, so each scene keeps clean
  corners even with a different crop.
- The radius belongs to the edited scene (0.4): the filter keeps one value per scene, in source
  pixels, and a "Rounded corners: scene" filter on that scene tells it which scene is rendering
  (it draws nothing). The same source keeps its own corners, or none, in other scenes. A scene
  duplicated by OBS (Studio Mode) keeps its original's corners; a scene duplicated by the user
  gets its own value the first time it is edited. A radius set for every scene (0.3, or the
  filter's own property) is split into per-scene values the first time one scene is edited.
  One Edit > Undo entry per change (a whole scrub included).

### Multiple selection
Header shows "N elements selected" and the number of locked items. Adds:

- **Align relative to** Selection | Canvas | Key object, with a key object picker. The key object is
  outlined with a thicker border on the Canvas and never moves. Default key object: the first
  selected element.
- **Distribute**: horizontal centers, horizontal spacing, vertical centers, vertical spacing
  (need 3 movable elements).
- **Spacing**: exact horizontal and vertical gap fields (need 2 movable elements). They show the
  current gap when all gaps are equal, otherwise *Mixed*. Elements are placed in spatial order
  from the first one, which stays.
- **Same width / height / size**: reference = key object when *Key object* is selected,
  otherwise the first selected element.

Every field shows *Mixed* when values differ. A typed value applies to every element; a relative
expression (`+20`) applies to each element's own value.

### Broadcast layouts (0.2)
The first section of the Inspector when something is selected, meant for people who only want a TV
layout without learning the rest. It counts the selected top-level sources as cameras or screens
(*Detect*: video capture devices, NDI, DeckLink and AJA are cameras, anything else is a screen; or
*All cameras*, *All screens*) and shows only the layouts made for that count, as thumbnails drawn
with the canvas shape (light blue cameras, amber screens, blue outline on the chosen one). With
nothing that fits, a short sentence says what to select.

| Guests | Cameras only | With screens |
|---|---|---|
| 1 | full screen | screen + PiP, screen + camera |
| 2 | duo split, duo 16:9 | screen + 2 side / below, 2 screens + 2 |
| 3 | three columns, host + 2 | screen + 3 side / below |
| 4 | 2 × 2, host + 3 below | screen + 4 side / below |
| 5 | 2 + 3, host + 4 | screen + 5 below |
| 6 | 3 × 2 | screen + 6 below |

Screens alone: one full, two side by side. Sources fill the slots in selection order. A camera fills
its tile edge to edge: the crop is centered horizontally and keeps 30 % of the removed height at the
top, so faces stay in frame. A screen is fitted whole, never cropped. A flipped source (mirrored
webcam, camera upside down) keeps its flip in the tile (0.3). On a portrait canvas guests
stack and screens span the width at the top; names that would be wrong change (three rows, stacked).

Settings: gap between tiles and margin around the layout (0 = full bleed, like TV). *Lock the layout
(nothing moves)*, on by default, locks the sources and gives them the *scale* constraints, so they
can't be dragged by accident and follow the canvas if its resolution changes; re-applying with the
box unchecked frees them. *Apply* arranges the current scene (one undo entry); *New scene* creates a
scene with the same sources in the layout and leaves the current one untouched. *Create show scenes*
makes all the scenes of a show at once: a full-screen scene per selected source (named after it), then
one scene per layout that fits the selection; the button briefly says how many were created. Hovering the section
previews the chosen layout on the Canvas. The Constraints section moves under a collapsed *Advanced*.

### Auto layout (0.2, multiple selection)
Direction Horizontal | Vertical | Grid; area *In the selection* or *In the canvas*; *Keep sizes* or
*Fill* (equal share of the main axis, ratio kept); alignment of the group on the main axis and of each
element on the cross axis (inside its cell for a grid); gap, padding, grid columns. Elements are placed
in reading order. Hovering the section shows the result as violet ghost frames on the Canvas;
*Apply layout* applies it as one undo entry.

### Presets (0.2)
The list shows the presets available for the number of selected elements, used in selection order:
corners (4), centered, full screen, lower third area (1 element); picture in picture, split screen,
stacked (2); one large + two small (3); 2 × 2 grid (4). Settings: margin, gap, corner size (percent of
the canvas width). Aspect ratios are kept, except full screen and the picture-in-picture background,
which fill the canvas. Hovering the section or an entry of the list previews on the Canvas.

### Constraints (0.2)
Collapsed under *Advanced*. Two lists: horizontal (OBS default, left, right, center, left and right, scale) and vertical (OBS
default, top, bottom, center, top and bottom, scale). Stored with each item in the scene collection.
When a canvas resolution changes, every constrained item of every scene of that canvas is placed
again: pinned edges keep their margin, center keeps the offset from the center, left and right (top
and bottom) stretch, scale follows the canvas. *OBS default* keeps OBS's own behavior (offset from the
center and size follow the canvas height).

### Locked elements
- Arrange (align, distribute, spacing, same size), auto layout, presets and mouse manipulation never
  move them. Applying a broadcast layout is explicit and does place them.
- They still count in the selection bounds.
- Explicit numeric edits in the Inspector apply to them, like OBS's *Edit Transform* window.

### Numeric fields
- Accept math (see grammar in `src/core/expression.hpp`):
  `1920 / 2`, `1080 - 120`, `(1+2)*3`, `50%`, `100% - 120`, `1,5` (comma decimal).
- Leading `+`, `-`, `*`, `x`, `/` = relative to the current value: `+20`, `-10`, `*1.5`, `/2`.
- Leading `=` forces an absolute value: `=-10`.
- `%` = percent of the canvas width (X, W) or height (Y, H), of 360 for rotation, of the source
  size for crop. After `*` or `/`, `%` is a fraction: `*50%` halves.
- `px`, `deg`, `°` suffixes are ignored.
- Enter or focus out applies, Escape reverts, Up / Down = ±1 (Shift ±10).
- Invalid input: red border, reason in the tooltip, value reverts after 1.5 s.
- Scrubbing: drag the field label horizontally (Shift ×10, Alt ×0.1). One undo entry per scrub.

### Reference point
The 3×3 picker chooses which point of the element X, Y, W, H refer to, and which point stays in
place when the size changes. Changing it never moves anything.

## Layouts Canvas

### Toolbar
Scene picker · zoom out · zoom % (menu: fit, 100 %, selection) · zoom in · Rulers · Guides ·
Grid ▾ (square, columns, rows, settings) · Snapping ▾ (guides, grid, elements, spacing, canvas) ·
Safe areas · Settings.

### Status bar
Canvas size · cursor X Y · selected count.

### Banners
Amber *PROGRAM* when the scene is on the program output; red *ON AIR* (and a red frame around the
canvas) when it is on the program while an output runs. When edits are blocked the banner says so
and refused actions show the reason.

### Mouse
| Action | Result |
|---|---|
| Click | select the topmost element (groups as a whole) |
| Shift + click | add / remove from the selection |
| Ctrl/Cmd + click | select inside a group |
| Double-click a group | select the child under the cursor |
| Double-click an element | open OBS's *Edit Transform* window |
| Drag an element | move the selection (locked elements stay) |
| Shift while dragging | constrain to the main axis |
| Ctrl/Cmd while dragging | bypass snapping (OBS convention) |
| Drag on empty canvas | rubber band selection (Shift adds) |
| Drag a handle | resize; corners keep the ratio, Shift frees it (OBS convention); Alt resizes from the center. With several elements selected, the handles are on the selection box and scale every element (0.2) |
| Drag just outside a corner | rotate around the center; Shift = 15° steps; snaps to 0/90/180/270 within 2° (Ctrl/Cmd disables) |
| Drag a guide | move it; drop it on a ruler to delete it |
| Double-click a guide | type its exact position (math accepted) |
| Drag from a ruler | create a guide (top ruler: horizontal, left ruler: vertical) |
| Space + drag, middle button drag | pan |
| Wheel / trackpad | pan; Shift + wheel pans horizontally |
| Ctrl/Cmd + wheel, pinch | zoom at the cursor |
| Alt (hold) | measure the selection to the hovered element, or to the canvas |
| Right-click | context menu: copy / paste properties, zoom to selection, guides |

### Keyboard (Canvas focused)
| Keys | Action |
|---|---|
| Arrows / Shift + arrows | move 1 / 10 px (consecutive moves merge into one undo entry) |
| C | crop mode for the selected item (0.2): orange handles crop the source, the image stays in place |
| Escape | cancel the current drag, leave crop mode, or clear the selection |
| Shift + 0 / 1 / 2 | zoom 100 % / fit / selection |
| + / − | zoom in / out |
| Shift + R | rulers |
| Shift + G | layout grid |
| Ctrl/Cmd + ; | guides |
| Ctrl/Cmd + Z, Ctrl/Cmd + Shift + Z | OBS undo / redo (OBS shortcuts, untouched) |

### Global hotkeys (0.2)
In OBS *Settings > Hotkeys*, under names starting with "Layouts:": show or hide rulers, guides, grid
and safe areas, turn snapping on or off, the six alignments, distribute horizontal and vertical
spacing, zoom the Canvas to fit. No key is bound by default; bindings are saved in the plugin
configuration. They work even when the Canvas does not have the focus.

### Rulers
Horizontal and vertical, in canvas pixels, adaptive 1-2-5 graduation (labeled ticks at least 64 px
apart, up to 10 subdivisions), negative values beyond the origin, the selection range highlighted,
the cursor position marked, guide markers. Crisp on HiDPI.

### Guides
- Scene guides (solid) and global guides (shown on every scene, lighter).
- Context menu on a guide: set position, lock / unlock, show on every scene / this scene only,
  delete. Context menu on the canvas: add vertical / horizontal guide at the cursor, add center
  guides, remove all guides of the scene, remove all global guides.
- Locked guides cannot be dragged.
- Guides snap to elements, grid and canvas while dragged (Ctrl/Cmd bypasses).
- Saved with the scene collection; never part of the video output.

### Grids
Square grid (size, offset, presets 8 / 10 / 16 / 20 / 32 / 64 px), column grid and row grid
(count, gutter, margin, stretch or fixed size aligned start / center / end). The grid display and
*Snap to grid* are independent. Square grid lines are hidden when closer than 6 screen pixels.

### Snapping
Targets in priority order: **guides > grid > elements (edges, centers, equal spacing) > canvas
(edges, center)**. Within the threshold (default 6 screen pixels, converted with the zoom), a
higher tier always wins, then the closest target. Feedback: red smart guide lines, and gap labels
for equal spacing. Resizing snaps the moving edges of unrotated elements.

### Safe areas
Action safe 3.5 %, title safe 5 %, 4:3 center-cut lines (OBS values), optional canvas center lines
and custom margins in pixels.

## States
Every control has default, hover, focus, pressed, disabled and (fields) mixed and invalid states,
inherited from the OBS theme and the tokens in `src/ui/theme.hpp`.

## Accessibility
Keyboard navigation in the Inspector (Tab, arrows in the reference picker), accessible names and
tooltips on every icon button, focus rings from the theme, minimum 24 px targets.
