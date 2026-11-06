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

/* Animated layout change inside one scene (0.6, the future "AUTO" of the Régie).
 *
 * An item is not interpolated field by field: OBS transforms mix scale, bounds, crop
 * and alignment, so a naive lerp makes the picture jump (crop moves the box, bounds
 * modes letterbox...). Instead each end is turned into a VisualState: the rectangle
 * that is really visible (center, size, rotation, flips, in parent space) plus the
 * window of the source that is shown (crop, in source pixels, clip-to-bounds
 * included). Both are interpolated, then a plain transform (center alignment, no
 * bounds) is derived for the frame. The last frame is the exact target transform.
 *
 * A flip that changes between the two ends goes through a zero width (card flip).
 * Rotation takes the shortest way. Pure C++, no libobs. */

#include "item-model.hpp"

#include <cstdint>
#include <vector>

namespace layouts::core {

/* Ease in-out cubic on [0, 1]. */
double easeInOutCubic(double t);

struct VisualState {
	Vec2 center;        /* parent space */
	double width = 0.0; /* visible size, parent units, >= 0 */
	double height = 0.0;
	double rotation = 0.0; /* degrees */
	bool flipX = false;
	bool flipY = false;
	/* Shown window of the source, in source pixels (fractional while morphing). */
	double cropLeft = 0.0, cropTop = 0.0, cropRight = 0.0, cropBottom = 0.0;
	uint32_t sourceWidth = 0;
	uint32_t sourceHeight = 0;
};

/* False when the source has no size yet (the item can only be lerped naively). */
bool hasVisualState(const ItemModel &m);
VisualState visualState(const ItemModel &m);

/* A transform showing exactly this state: center alignment, no bounds, integer crop
 * (the visible size stays exact, the shown window moves by less than a pixel). */
ItemTransform transformFor(const VisualState &v);

/* t in [0, 1], already eased. */
VisualState interpolate(const VisualState &a, const VisualState &b, double t);
ItemTransform interpolate(const ItemModel &a, const ItemModel &b, double t);

/* ---- Plan of a whole scene --------------------------------------------------- */

struct MorphItemState {
	int64_t id = 0;
	ItemModel model;
	bool visible = true;
	double opacity = 1.0; /* current fade level (a morph interrupted mid-way) */
};

enum class TrackKind {
	Move,  /* visible at both ends: glides to its new place */
	Enter, /* hidden (or absent) before, visible after: fades in at its target place */
	Leave, /* visible before, hidden after: fades out where it is */
};

struct MorphTrack {
	int64_t id = 0;
	TrackKind kind = TrackKind::Move;
	ItemModel from;
	ItemModel to;
	double opacityFrom = 1.0;
	double opacityTo = 1.0;
};

struct MorphPlan {
	std::vector<MorphTrack> tracks;
	bool empty() const { return tracks.empty(); }
};

/* targets: the wanted state of the items of the layout (visible = false asks the item
 * to leave). An item of `current` that is not in targets is left alone. Items whose
 * state does not change get no track. */
MorphPlan planMorph(const std::vector<MorphItemState> &current, const std::vector<MorphItemState> &targets);

struct MorphFrameItem {
	int64_t id = 0;
	ItemTransform t;
	double opacity = 1.0;
};

/* progress in [0, 1] (linear time); easing is applied here. At 1 every transform is
 * exactly the target one. */
std::vector<MorphFrameItem> sampleMorph(const MorphPlan &plan, double progress);

} // namespace layouts::core
