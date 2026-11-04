# UX specification

Layouts brings design-tool composition to OBS while keeping OBS conventions where they already
exist. This document describes the behavior of version 0.1.0.

## Docks

| Dock | Default placement | Purpose |
|---|---|---|
| Inspector | docked on the right, shown on first launch | properties of the selection |
| Layouts Canvas | floating, 1100 × 700, shown on first launch | visual editing with rulers and guides |

Both are standard OBS docks (*Docks* menu): movable, resizable, dockable, floatable, closable.

## Edited scene

The Canvas toolbar has a scene picker:

- **Follow OBS (scene name)** (default): the preview scene in Studio Mode, otherwise the program
  scene. Changes when OBS changes scene.
- **Any scene**, including scenes of other OBS 33 canvases (shown as `Canvas › Scene`): pinned
  until *Follow OBS* is chosen again or the scene is deleted.

The Inspector always shows the selection of the edited scene.

## Inspector

### Empty state
Scene name, canvas size and a hint on how to select.

### Single selection
| Section | Controls |
|---|---|
| Header | name, source type · scene › group, Lock, Copy properties, Paste properties ▾ |
| Align | 6 buttons: left, horizontal center, right, top, vertical center, bottom, relative to the canvas |
| Position | reference point picker (3×3), X, Y, rotation, rotate ±90°, flip H/V |
| Dimensions | W, H, ratio lock, Fit, Fill, Stretch, Original size, Reset transform |
| Crop | left, right, top, bottom (source pixels) |
| Bounding box | type, content alignment, crop to bounds |

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
Two lists: horizontal (OBS default, left, right, center, left and right, scale) and vertical (OBS
default, top, bottom, center, top and bottom, scale). Stored with each item in the scene collection.
When a canvas resolution changes, every constrained item of every scene of that canvas is placed
again: pinned edges keep their margin, center keeps the offset from the center, left and right (top
and bottom) stretch, scale follows the canvas. *OBS default* keeps OBS's own behavior (offset from the
center and size follow the canvas height).

### Locked elements
- Arrange (align, distribute, spacing, same size) and mouse manipulation never move them.
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
