# Changelog

## Unreleased (0.2 in progress)

- Resize several selected elements at once from the selection handles on the Canvas (corners keep the
  ratio, Shift frees it, Alt resizes from the center, edges snap).
- Auto layout in the Inspector: horizontal, vertical or grid; gap, padding, alignment on both axes,
  keep sizes or fill with equal sizes; inside the selection or the whole canvas; applied on demand.
- Composition presets: corners, centered, full screen, lower third area, picture in picture, split
  screen, stacked, one large + two small, 2 × 2 grid; margin, gap and corner size settings.
- Ghost preview on the Canvas while hovering the auto layout or presets section, before applying.
- Crop mode on the Canvas (C or the context menu): the handles crop the source and the image stays
  in place. Items without a bounding box.
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
