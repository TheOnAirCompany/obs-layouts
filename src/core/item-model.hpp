/*
Layouts for OBS
Copyright (C) 2026 The OnAir Company

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#pragma once

/* A faithful, libobs-free model of an OBS scene item transform.
 *
 * computeGeometry() mirrors update_item_transform() and calculate_bounds_data()
 * from libobs/obs-scene.c (OBS 33.0.1) using the absolute values returned by
 * obs_sceneitem_get_info2() / obs_sceneitem_get_crop(). Every editing operation
 * of Layouts is computed here first, so results never depend on when libobs
 * refreshes its cached matrices. See docs/COORDINATE-SYSTEM.md. */

#include "geometry.hpp"

#include <cstdint>
#include <string>

namespace layouts::core {

/* Same bit values as OBS_ALIGN_* in libobs/obs-defs.h. */
enum AlignFlags : uint32_t {
	kAlignCenter = 0,
	kAlignLeft = 1u << 0,
	kAlignRight = 1u << 1,
	kAlignTop = 1u << 2,
	kAlignBottom = 1u << 3,
};

/* Same order and values as enum obs_bounds_type. */
enum class BoundsType : int {
	None = 0,
	Stretch = 1,
	ScaleInner = 2,
	ScaleOuter = 3,
	ScaleToWidth = 4,
	ScaleToHeight = 5,
	MaxOnly = 6,
};

struct Crop {
	int left = 0;
	int top = 0;
	int right = 0;
	int bottom = 0;

	bool operator==(const Crop &o) const
	{
		return left == o.left && top == o.top && right == o.right && bottom == o.bottom;
	}
	bool operator!=(const Crop &o) const { return !(*this == o); }
};

struct ItemTransform {
	Vec2 pos;
	Vec2 scale{1.0, 1.0};
	double rot = 0.0; /* degrees, clockwise on screen */
	uint32_t align = kAlignLeft | kAlignTop;
	BoundsType boundsType = BoundsType::None;
	uint32_t boundsAlign = kAlignCenter;
	Vec2 bounds;
	bool cropToBounds = false;
	Crop crop;
};

struct ItemModel {
	ItemTransform t;
	uint32_t sourceWidth = 0;
	uint32_t sourceHeight = 0;
};

struct ItemGeometry {
	uint32_t croppedWidth = 0;  /* source width after crop */
	uint32_t croppedHeight = 0; /* source height after crop */
	Vec2 boxScale;              /* obs_sceneitem_get_box_scale(): signed box size */
	Affine box;                 /* obs_sceneitem_get_box_transform(): unit square -> parent */
	Affine draw;                /* obs_sceneitem_get_draw_transform(): cropped source px -> parent */
	Vec2 outputScale;
};

ItemGeometry computeGeometry(const ItemModel &m);

/* Visual frame of an item: the box with flips removed, so that ref (0,0) is always
 * the corner that looks top-left once rotation is undone. Expressed in parent space. */
struct VisualFrame {
	Quad quad;       /* tl, tr, br, bl in visual order */
	double width;    /* |box size x|, in parent units */
	double height;   /* |box size y| */
	double rotation; /* degrees */

	Vec2 point(const RefPoint &ref) const;
	Vec2 center() const { return quad.center(); }
};

/* parent maps the item's parent space (scene or group) to world (canvas) space. */
VisualFrame visualFrame(const ItemModel &m, const Affine &parent = Affine::identity());

/* ---- Editing operations ------------------------------------------------------
 * All take world-space values and return a new transform. "anchor" is the point of
 * the visual frame that must not move. */

ItemTransform moveBy(const ItemModel &m, const Affine &parent, const Vec2 &worldDelta);
ItemTransform moveRefTo(const ItemModel &m, const Affine &parent, const RefPoint &ref, const Vec2 &worldTarget);

/* Sets the visual (world) size. With bounds the bounds box is resized; without bounds
 * the scale is changed, flips are preserved. Width or height <= 0 are ignored. */
ItemTransform setWorldSize(const ItemModel &m, const Affine &parent, double width, double height,
			   const RefPoint &anchor);

/* How the content fills its frame when the frame changes shape (0.4).
 *   Fill    scales uniformly to cover the frame and crops what goes past it
 *   Fit     shows the whole source, scaled uniformly inside the frame (bounding box)
 *   Stretch scales each axis to the frame (the image is distorted) */
enum class FrameMode { Fill, Fit, Stretch };

/* Gives the item a visible frame of width x height (world units). The anchor is both the
 * point of the frame that stays in place and, for Fill and Fit, the part of the image
 * that stays (top-left keeps the top-left of the picture...). When the frame keeps its
 * shape, the item is only scaled (a manual crop is kept). Flips and rotation are kept. */
ItemTransform setFrame(const ItemModel &m, const Affine &parent, double width, double height, FrameMode mode,
		       const RefPoint &anchor);

/* The largest frame with this width / height ratio inside the current frame. */
ItemTransform setFrameRatio(const ItemModel &m, const Affine &parent, double ratio, FrameMode mode,
			    const RefPoint &anchor);

ItemTransform setWorldRotation(const ItemModel &m, const Affine &parent, double deg);
ItemTransform rotateBy(const ItemModel &m, const Affine &parent, double deltaDeg);
ItemTransform flip(const ItemModel &m, const Affine &parent, Axis axis);

/* Without bounds the visible content stays in place; with bounds the anchor stays. */
ItemTransform setCrop(const ItemModel &m, const Affine &parent, const Crop &crop, const RefPoint &anchor);

enum class FitMode { Fit, Fill, Stretch };
/* Fits the unrotated visual frame into `area` (world space) and centers it. */
ItemTransform fitInto(const ItemModel &m, const Affine &parent, const Rect &area, FitMode mode);

/* Scale 1:1, no bounds, anchored. */
ItemTransform originalSize(const ItemModel &m, const Affine &parent, const RefPoint &anchor);

/* Changes OBS bounds settings while keeping the anchor in place. Switching from no
 * bounds to bounds initializes the bounds box to the current visual size. */
ItemTransform setBoundsType(const ItemModel &m, const Affine &parent, BoundsType type, const RefPoint &anchor);

/* ---- Canvas resize handles ---------------------------------------------------
 * start is the frame when the drag began, handle the grabbed handle (fx and fy in
 * {0, 0.5, 1}) and pointer where that handle is now, in the same space as start.
 *   keepRatio   corners and edges keep the ratio of start; an edge handle grows the
 *               other axis on both sides of its center (like Figma)
 *   fromCenter  the center stays in place, otherwise the opposite side or corner
 * The frame never turns inside out: width and height stay >= minSize (the ratio is
 * kept while clamping). anchor is the point of start that does not move, as a
 * fraction of the frame (the same fraction of the result). */
struct HandleResize {
	Rect rect;
	RefPoint anchor;
};
HandleResize resizeWithHandle(const Rect &start, const RefPoint &handle, const Vec2 &pointer, bool keepRatio,
			      bool fromCenter, double minSize = 1.0);

/* Short ratio text for the resize badge: "16:9", "4:3", "1:1", "9:16"... when the ratio
 * is close to a common one (or to a small integer ratio), otherwise "1.85:1". */
std::string ratioLabel(double width, double height);

/* Mirrors obs_sceneitem_get_box_transform() for a parent transform made of OBS groups. */
double worldRotation(const ItemModel &m, const Affine &parent);

} // namespace layouts::core
