# UX specification

Layouts brings design-tool composition to OBS while keeping OBS conventions where they already
exist. This document describes the behavior of version 0.5.0; sections added after 0.1.0 give the
version that brought them.

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

### Scenes (0.4)
Above Layers, in a resizable split: "Follow OBS (scene)" then every scene (other canvases as
"Canvas › Scene"). Clicking a scene edits it in the window (Layers, Canvas, Inspector) without
changing the program or the preview; the first row goes back to following OBS. The scene on the
program has a red dot (in Studio Mode, the scene whose private copy is on the program). Same
choice as the scene menu of the Canvas toolbar, which stays.

### Types and filter (0.4)
- A pill after the name shows the source's type: HOST, CAM, SCREEN, CHAT (0.5), BG, CG, or a
  dashed "?" when not set. Clicking it opens the type menu; when the row is selected, the type applies to every
  selected source. One undo entry. Groups have no type.
- The type is saved in the source's private settings (scene collection), so renaming keeps it.
  Until set, it is suggested from the name prefix (Host: HOST, PRÉSENTATEUR, PRESENTER, ANIMATEUR;
  Cam: CAM, CAMERA, CAMÉRA, GUEST, INVITÉ; Screen: SCREEN, ÉCRAN, SCR, DISPLAY, CAPTURE; BG: BG,
  BACKGROUND, FOND; CG: CG, GFX, OVERLAY, LOGO, LT, HABILLAGE; Chat (0.5): CHAT, TCHAT, CHATBOX),
  case and accents ignored, the
  prefix followed by the end of the name, a space, `_`, `-` or `.`; the longest prefix wins. Then
  from the kind of source (video capture, NDI, DeckLink, AJA = Cam; display, window, game capture
  = Screen). A chat is only suggested from its name, never from the kind of source.
- Filter row above the list: All, then one chip per type present, with counts, in the order Host,
  Cam, Screen, Chat, BG, CG, Not set. Groups stay while
  one of their sources matches. Drag and drop is off while a filter is active.

## Layouts Builder (0.4)
Tools > Layouts Builder, or the Scene | Builder switch at the top left of the window (Tools >
Layouts opens Scene mode; the hotkey keeps the current mode). The Canvas stays in the center.
- Left: "Sources of the collection", every video input once (scenes and groups excluded): source
  icon, name, number of scenes that use it, type pill (same menu as Layers). Same filter row.
  Select the sources of the scene to make (Cmd / Shift for several, Cmd+A for all).
- Right: "Broadcast layouts", the same gallery as in Scene mode for the selection (hosts and cams
  are cameras, screens are screens, chats are chats; sources without a type are ignored and
  counted), gap and
  margin (shared), "Background at the back" and "Graphics on top" (enabled when one is selected),
  lock. "New scene" (or a double-click on a layout) makes the chosen layout; "Create show scenes"
  makes a full-screen scene per host, cam and screen, then one per layout that fits. The new
  scene opens on the Canvas.
- Each scene: the first BG at the back, covering the canvas (bounding box "scale to outer bounds",
  centered, cropped to bounds); screens, then hosts and cams in their tiles (hosts first, so they
  get the large tile), flips kept; CG on top with the transform and crop they have in the first
  scene that uses them. Locked when "Lock" is checked. Names made unique ("Duo 2").
- 0.5: host slots get a Host source first; items are added back to front in the layout's stacking
  order (see Broadcast layouts), chats after cameras, graphics last; slot rotation, shape and frame
  are written as the item's style. Create show scenes makes no full-screen scene for a chat.

## Inspector

### Empty state
Scene name, canvas size and a hint on how to select.

### Single selection
| Section | Controls |
|---|---|
| Header | name, source type · scene › group, Lock, Copy properties, Paste properties ▾ |
| Align | 6 buttons: left, horizontal center, right, top, vertical center, bottom, relative to the canvas |
| Position | reference point picker (3×3), X, Y, rotation, rotate ±90°, flip H/V |
| Dimensions | W, H, ratio lock, Fit, Fill, Stretch, Original size, Reset transform, frame mode Fill / Fit / Stretch and ratio buttons 16:9, 1:1, 4:5, 9:16 (0.4) |
| Style (0.5) | corners Auto / px / Round / None + px field, border + width + color, shadow None / Soft / Hard / Custom (+ blur, opacity, color, X, Y), fade during transitions; "modified" dot, Reset to type, Styles by type… |
| Crop | left, right, top, bottom (source pixels) |
| Bounding box | type, content alignment, crop to bounds |

#### Frame modes (0.4)
- A segmented row Fill | Fit | Stretch, saved on each scene item (private settings
  `layouts_frame`, Fill by default); nothing checked when the selection is mixed. Picking a mode
  refills the current frame of the selection (one undo entry).
- W and H (typed or scrubbed) set the visible frame. Fill: uniform scale covering the frame, the
  rest cropped in source pixels (a tiny per-axis correction absorbs the whole-pixel crop). Fit:
  bounding box "scale to inner bounds" of the frame, crop cleared, content aligned on the
  reference point. Stretch: per-axis scale, as before. When the frame keeps its shape (ratio lock
  on), Fill only scales, so a manual crop is kept.
- The 3×3 reference point is the point of the frame that stays in place and, for Fill, the part
  of the image kept (top-left keeps the top-left of the picture; mirrored axes are handled).
- Ratio buttons 16:9, 1:1, 4:5, 9:16: the largest frame of that shape inside the current frame,
  anchored on the reference point, filled with the item's mode. Locked items are skipped.
- The Canvas handles use the frame mode when they resize freely (0.5, see Handles).

#### Corner radius (0.3)
Replaced in 0.5 by the Corners row of the Style section (below); what follows still describes how
the corners filter renders.
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
  Transitions that draw each source themselves instead of the scenes (Move Transition) run no
  scene filter: the corner filter then takes the transition on the program (remembered from
  the UI thread on scene and transition changes), the radius of the source in its scene A and
  in its scene B, and blends them with the transition time eased cubic in and out (Move's
  default); a source in only one of the two scenes keeps that scene's radius.
  One Edit > Undo entry per change (a whole scrub included).

#### Style (0.5)
A section right after Dimensions. Everything it sets belongs to the scene item (so the same source
can look different in two scenes) and is drawn by Layouts' filters: the "Rounded corners" filter
(corners and diagonal masks) and a "Border and shadow" filter kept last in the source's filters.
Groups have no style.
- **Corners**: segmented Auto | px | Round | None, and a px field. Auto = 4 % of the short side of
  the visible frame, between 6 and 48 px; px = a fixed radius in canvas pixels (typing a value
  switches to px; `+4` applies to each item's own radius); Round = half the short side (a circle or
  a pill); None = square. A radius is never more than half the short side. Auto and Round follow
  the size of the item when it is resized or cropped, rotated items included.
- **Border**: checkbox, width (canvas px) and color swatch (color picker with alpha). Turning it on
  gives 4 px white. The border is drawn inside the frame and follows the corners.
- **Shadow**: segmented None | Soft | Hard | Custom. Soft = blur 16, offset 0 / 6, 25 %; Hard =
  blur 6, offset 0 / 6, 60 %; Custom shows blur, opacity, color, X and Y. The shadow is drawn behind
  the item only (a translucent item does not darken itself), above the items below it, and keeps
  falling down the canvas when the item is rotated.
- **Fade during transitions** (enabled when there is a shadow): while a transition that draws the
  sources itself (Move Transition) runs on the program, the shadow fades out and comes back
  (1 → 0 at mid-transition → 1, eased). Corners blend from scene A to scene B as in 0.4.
- **Styles by type…** (settings icon in the section title) opens a dialog: the list of types (pills)
  and the same editor. Each type has a default style, saved with the scene collection; a type
  never set uses corners Auto, no border, no shadow. Changing a type default re-renders every item
  of that type, in every scene and group of the collection, that has no style of its own.
- **Inheritance**: an item follows its type until a field is edited in the Inspector; it then has
  its own style and the title shows the "modified" dot (accent color). **Reset to type** (undo
  icon) drops the item's own style and it follows its type again. Re-applying a type default never
  turns an item into an overridden one.
- Slots of a broadcast layout write their shape into the item's style: a round slot sets corners
  Round, a diagonal slot sets a mask polygon (corners None), a framed slot turns the border on
  (Polaroid). Moving the item to a plain slot removes only what a slot added.
- Multiple selection: a change applies only the edited field to each item, so each keeps the rest
  of its style. Controls show a mixed state (nothing checked, tri-state checkbox, "Mixed", dashed
  swatch).
- Every edit, and Reset to type, is one Edit > Undo step ("Style in '<scene>'").

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

Screens alone: one full, two side by side. Sources fill the slots in selection order.

0.5 brings the 16:9 catalog to 118 layouts: 33 more with cameras only (strips, cinema, host in L,
guest PiP...), 42 more with screens (screens alone, screens + cameras: float, column, bubble,
screen over a full camera, screen PiP...) and 20 inventive ones (tilted cards, Polaroid, bento 4 and
6, dock of bubbles, diagonal duo, card stack, tilted screen, masonry wall, porthole, and 10 with a
chat: chat + camera, chat + screen + camera, chat duo, chat card, ticker, chat + guests, questions,
visio, podcast, gameplay). These 95 are drawn for landscape canvases only and are hidden from the
gallery on a portrait canvas.
- **Chats**: the gallery counts cameras, screens and chats and shows only the layouts with that
  exact number of chats. A chat is fitted whole in its slot (never cropped, never flipped, sized
  even before a browser source has loaded): at the top of a column, centered in a ticker.
- **Slot shapes**: a slot can be rotated (the item turns around the center of its cell and still
  covers it), round (radius 50 %), cut on a diagonal (convex mask polygon, anti-aliased like the
  corners), or framed (Polaroid border). Thumbnails draw all of them, with chats in the chat color.
- **Host**: a slot can be made for the host; Host sources fill host slots first, the others take
  the remaining slots in order.
- **Stacking**: slots are stacked back to front in the order of the layout, except that a slot
  strictly inside a later, larger slot goes in front of it (picture in picture). The items of the
  layout reuse the stack positions they already had, so a background stays below them and graphics
  stay above.
- **Mirror**: two toggles ↔ and ↕ above the gallery (Inspector and Builder) mirror every layout left
  / right and top / bottom: thumbnails, Canvas preview, Apply, New scene and Create show scenes.
  Only the slots move: the image of a source is never flipped; a rotation is reversed when one axis
  is mirrored. The state is remembered. A camera fills
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

#### Save as layout (0.5)
*Save as layout…* (Inspector broadcast section, gallery and Canvas context menus, Scene mode) asks
for a name and saves the scene as a layout of the gallery: the selected sources, or every visible
top-level camera, screen and chat when nothing is selected. Each tile is the visual frame of the
item (crop, bounding box, rotation and groups included), relative to the canvas. Host and Cam give
camera tiles (host tiles for Host), Screen gives screens, Chat gives chats (a chat wider than 4:1 is
a ticker); BG and CG are skipped. User layouts are saved in the plugin configuration, so every scene
collection sees them, and are listed after the built-in ones when the selection has the same counts;
they are stretched to the canvas. In the gallery they carry an accent dot at the top right and the
tooltip "(your layout)". Right-click a user layout: Rename…, Export…, Delete (asks first; scenes
made with it do not change); anywhere in the gallery: Save as layout…, Import layouts…, Export all my
layouts…. Import reads a `layouts-user-layout/1` JSON file: a layout already there is skipped, a
name already taken gets a suffix ("Duo (2)").

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
| Drag a handle | resize, following the ratio lock (0.5, see Handles); Shift inverts the lock, Alt resizes from the center. With several elements selected, the handles are on the selection box and scale every element (0.2) |
| Drag an element onto another of the same kind | swap their slots (0.5, see Swap); Alt = plain move |
| Drag just outside a corner | rotate around the center; Shift = 15° steps; snaps to 0/90/180/270 within 2° (Ctrl/Cmd disables) |
| Drag a guide | move it; drop it on a ruler to delete it |
| Double-click a guide | type its exact position (math accepted) |
| Drag from a ruler | create a guide (top ruler: horizontal, left ruler: vertical) |
| Space + drag, middle button drag | pan |
| Wheel / trackpad | pan; Shift + wheel pans horizontally |
| Ctrl/Cmd + wheel, pinch | zoom at the cursor |
| Alt (hold) | measure the selection to the hovered element, or to the canvas |
| Right-click | context menu: copy / paste properties, zoom to selection, guides |

### Handles (0.5)
- The ratio lock of the Inspector decides, for corner and edge handles alike. Locked: a corner
  scales along the diagonal (the pointer is projected on it, so there is no dead zone); an edge
  keeps the ratio and the other axis grows around its middle. Unlocked: corners and edges are free.
- Shift inverts the lock for the drag; Alt resizes from the center. Pressing or releasing either
  applies at once, without moving the mouse.
- A locked resize only scales, so a manual crop is kept. A free resize refills the new frame with
  the item's frame mode: Fill crops (the crop is recomputed from the whole source, replacing a manual
  crop), Fit letterboxes, only Stretch distorts. Groups always scale.
- With several elements selected, the same rules apply to the selection box, each element filled
  with its own frame mode when free.
- A badge next to the cursor shows the ratio while a locked resize runs ("16:9", "1:1", "1.85:1").
- The frame never turns inside out: dragging past the opposite side stops at 1 px.

### Swap (0.5)
- While one element is dragged, the topmost compatible element under the cursor is the target:
  Host and Cam swap with each other, Screen with Screen, Chat with Chat; BG, CG and untyped sources
  never swap. The target shows a 2 px accent outline, a 12 % accent fill and the ⇄ icon.
- On release the dragged element goes back and the two exchange their slots. What stays with the
  slot: the cell and its rotation, the stacking position, the style, the lock and the constraints.
  What stays with the source: flips and frame mode. Each source is refitted to its new cell (cover
  crop keeping the top for cameras, whole for screens and chats).
- A locked element does not move but can still be swapped: the drag only looks for a target. So a
  locked broadcast layout can be rearranged without unlocking it.
- Holding Alt, or snapping to a guide, turns swapping off: the drag is a plain move.
- One Edit > Undo step ("Swap"), stacking and style included. Edits on air follow the
  block-on-air policy.

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
configuration. They work even when the Canvas does not have the focus. The Switcher adds its own
entries (0.5, see below).

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

## Layouts Switcher (0.5)
A dock, **Docks > Layouts Switcher** (French: *Régie Layouts*), to recompose one scene live like a
vision mixer.
- **Plateau**: a combo picks the scene that holds every camera, screen and chat of the show (saved
  with the collection). The Switcher reads its top-level items by type; graphics, background and
  untyped items are never touched. Sources a layout does not use are hidden, never removed.
- **LAYOUTS**: one button per layout that fits the Plateau's sources (thumbnail, numbered 1 to 9
  for the hotkeys); layouts drawn for landscape are left out on a portrait canvas.
- **BUS rows**: one per slot of the layout in preview ("BUS 1 · camera"), one button per source.
  Clicking a source puts it in the slot; if it already is in another slot, the two swap.
- **PREVIEW** line: what will go on air. **CUT** sends the preview to program with the current
  Studio Mode transition. **AUTO** is greyed (tooltip "0.6") until animated layout changes land.
- **Tally**: red = on air, green = in preview, red with a green underline = both, on layout and
  source buttons. Compact density: 28 px buttons.
- **Studio Mode**: with OBS's "duplicate scene" (the default), the Plateau is prepared as the
  preview and CUT takes it. Without duplication the next composition is prepared on a twin scene
  "<Plateau> B" (same sources, kept in sync), alternating with the Plateau. With Studio Mode off,
  every change goes on air at once and the dock shows a banner with an "Enable Studio Mode" button.
- Layouts are applied as in the Inspector: flips kept, cameras cover their cell, screens and chats
  whole, style and stacking from the slots.
- **Hotkeys** (Settings > Hotkeys, "Switcher: ..."): cut (take), next layout, previous layout,
  layout 1 to 9, slot N = source M (slots 1 to 4, sources 1 to 8 in the Plateau order from the
  top). No key bound by default.
- **obs-websocket**: vendor `layouts`, requests `GetState`, `ListLayouts`, `SetLayout {key}`,
  `AssignSlot {slot, source}` (slot from 1), `Swap {a, b}` (slots from 1) and `Take`; event
  `StateChanged` with the full state after every change. Requests run on the UI thread. Meant for
  Stream Deck, Bitfocus Companion and scripts.

### Lost source failover (0.5)
- While a layout from the Switcher is on program, its cameras and screens are watched (every
  250 ms): a source is lost when it has no picture any more (size 0, as when a capture device is
  unplugged or an NDI sender stops), when its media ends or fails, or when it is deleted.
- After 1.5 s lost, the program drops to the layout with one tile less: the same family when
  there is one (same chats), otherwise the first one that fits, the remaining sources keeping
  their order (the host stays in the host slot). The lost item is hidden and the others move, in
  one atomic change on the program, without a transition. A drop seen while the scene is not on
  program is applied when it goes on air.
- The Switcher shows a red banner under its title ("CAM 2 lost: layout reorganized on program",
  or "hidden on program" when no layout with one tile less exists).
- When every lost source is back and stable for 2 s, the exact original layout returns by itself,
  also without a transition, and the banner turns green ("Source back: original layout
  restored") for a few seconds.
- Changing the scene collection or quitting OBS restores the original layout first.
- Checkbox "Reorganize the program when a camera or a screen drops" under CUT, on by default;
  turning it off during a failover restores the original layout.
- A frozen last frame (a sender that keeps showing its last picture) is not detected.

## States
Every control has default, hover, focus, pressed, disabled and (fields) mixed and invalid states,
inherited from the OBS theme and the tokens in `src/ui/theme.hpp`.

## Accessibility
Keyboard navigation in the Inspector (Tab, arrows in the reference picker), accessible names and
tooltips on every icon button, focus rings from the theme, minimum 24 px targets.
