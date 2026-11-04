# Changelog

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
