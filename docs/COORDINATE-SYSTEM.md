# Coordinate system

This document defines every number Layouts shows or edits. All formulas are implemented in
`src/core/item-model.cpp` and covered by `tests/test-item-model.cpp` and by the in-OBS
self-test `geometry-fidelity`, which compares them with the matrices libobs computes.

## Spaces

| Space | Unit | Origin | Used for |
|---|---|---|---|
| Source | source pixel | top-left of the uncropped source | crop values |
| Cropped source | source pixel | top-left after crop | OBS draw transform input |
| Parent | canvas pixel of the parent | scene top-left, or group top-left | OBS `pos`, `scale`, `bounds` |
| World (canvas) | canvas pixel | top-left of the edited scene's canvas | everything shown in Layouts |
| View | logical screen pixel | top-left of the Canvas widget | mouse, rulers |
| Device | physical pixel | top-left of the display | GPU drawing (view × devicePixelRatio) |

`world = pan + view / zoom`, `device = view × devicePixelRatio`. The zoom is expressed in
logical pixels per canvas pixel, so "100 %" shows one canvas pixel per logical pixel (two
device pixels on a Retina screen), like design tools.

Y points down everywhere. A positive rotation is clockwise on screen, exactly like OBS.

## The OBS item transform

Layouts reads the absolute values returned by `obs_sceneitem_get_info2()` and
`obs_sceneitem_get_crop()` (OBS 31+ stores relative values internally; the getters convert them).

With `cx, cy` the cropped source size, `S` the scale, `R` the rotation, `P` the position and
`O` the alignment offset, libobs computes:

```
draw = T(P) · R(rot) · T(−O_draw) · S(scale')          cropped source px → parent
box  = T(P) · R(rot) · T(−O_box)  · S(boxScale)        unit square      → parent
```

- Without bounds: `scale' = scale`, `boxScale = scale ⊙ (cx, cy)`.
- With bounds: `scale'` is derived from the bounds type (inner, outer, width, height, stretch,
  max only) and `boxScale = bounds`.
- `O = alignmentOffset(align, (int)boxScale)`; center alignment uses an integer division by 2,
  reproduced bit for bit.
- libobs converts possibly negative floats to `uint32_t` in the no-bounds path. That conversion is
  undefined in C; `core::computeGeometry()` reproduces what the shipping compilers emit
  (saturation on arm64, 64-bit wrap on x86-64). Measured error against libobs on macOS arm64:
  **0 to 0.00013 px** on 10 representative transforms.

Group children: `world = groupDraw · box`, where `groupDraw` comes from
`obs_sceneitem_get_draw_transform(group)`.

## The visual frame

The **visual frame** of an item is its box in world space with flips removed, so that the
reference point (0, 0) is always the corner that looks top-left once rotation is undone. Its four
corners are `tl, tr, br, bl`; its width and height are the lengths of `tr − tl` and `bl − tl`.

- **W / H** = visual width / height = what you see, after scale, crop and bounds. With a
  bounding box, W/H are the bounding box (the frame OBS draws), not the possibly smaller
  content inside it.
- **X / Y** = world position of the selected **reference point** of the visual frame:
  `point(fx, fy) = tl + (tr − tl)·fx + (bl − tl)·fy`, with `fx, fy ∈ {0, 0.5, 1}`.
  Changing the reference point only changes what is displayed; nothing moves.
- **Rotation** = item rotation + parent (group) rotation, normalized to (−180°, 180°].
- **Measurements, alignment, distribution and snapping** use the axis-aligned bounding box of
  the visual frame (for rotated items this is larger than the item, as in Figma).
- **Crop** values are in source pixels.

## Editing operations

All operations compute a new transform in the core, then apply it in one atomic transaction.

| Operation | Rule |
|---|---|
| Move | `pos += parent⁻¹ · Δworld` |
| Set X / Y | move so that the reference point lands on the target |
| Set W / H | without bounds: `scale = sign(scale) · W_local / cx`; with bounds: `bounds = (W, H)`; then the reference point is put back where it was |
| Ratio lock | the other dimension follows the current aspect ratio |
| Rotation | rotate, then put the visual center back where it was |
| Flip | negate the scale on that axis, keep the center |
| Crop | without bounds, the source pixels stay where they are on the canvas; with bounds, the reference point stays. Crop never removes the whole source |
| Fit / Fill / Stretch | fit the unrotated visual frame into the canvas, then center it |
| Original size | scale ±1, no bounds, reference point kept |
| Bounds type | none → bounds: bounds = current visual size; bounds → none: scale = the scale the bounds were applying, so the content does not jump |

## Precision

- The core works in double precision and never accumulates: drags always start from the
  transform captured at mouse press.
- When OBS stores relative coordinates it rounds absolute positions to 0.5 px
  (`nudge_abs_values`); values typed in Layouts are therefore precise to 0.5 px for top-level items.
- Moves are rounded to whole canvas pixels (0.5 px when zoomed in at 400 % or more). Resizes are
  rounded to whole pixels. Numeric fields keep the decimals you type.
