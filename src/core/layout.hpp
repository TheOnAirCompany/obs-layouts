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

/* Layout engines that compute target frames for several items at once:
 *  - scaling a selection (multi-selection resize on the Canvas),
 *  - auto layout (horizontal, vertical, grid) applied on demand,
 *  - composition presets (corners, picture in picture, split screen...).
 *
 * Items are described by their visual frame: center, unrotated width and height.
 * Results are new frames in the same order; callers keep rotation as it is. */

#include "geometry.hpp"
#include "item-model.hpp" /* FitMode */

#include <optional>
#include <string>
#include <vector>

namespace layouts::core {

struct Frame {
	Vec2 center;
	double width = 0;
	double height = 0;

	Rect rect() const { return {center.x - width / 2, center.y - height / 2, width, height}; }
	static Frame fromRect(const Rect &r) { return {r.center(), r.w, r.h}; }
	double aspect() const { return height > 0 ? width / height : 1.0; }
};

/* ---- selection scaling ---- */

/* Maps every frame from `from` to `to` (both axis-aligned): centers are mapped
 * proportionally, sizes are multiplied by the scale of each axis. */
std::vector<Frame> scaleFrames(const std::vector<Frame> &frames, const Rect &from, const Rect &to);

/* ---- fitting ---- */

/* Largest frame with the given aspect ratio that fits (Fit) or covers (Fill) the
 * cell, centered; Stretch returns the cell. */
Frame fitInCell(double aspect, const Rect &cell, FitMode mode);

/* ---- auto layout ---- */

enum class FlowDirection { Horizontal, Vertical, Grid };
enum class FlowAlign { Start, Center, End };
enum class FlowSizing { Keep, Fill };

struct FlowSpec {
	FlowDirection direction = FlowDirection::Horizontal;
	double gap = 16;                         /* between items (main axis; both axes for grids) */
	double padding = 0;                      /* inside the container */
	FlowAlign mainAlign = FlowAlign::Start;  /* where the packed group sits on the main axis */
	FlowAlign crossAlign = FlowAlign::Start; /* items on the cross axis (grid: inside their cell) */
	FlowSizing sizing = FlowSizing::Keep;    /* Fill: equal share of the main axis, ratio kept */
	int columns = 2;                         /* grid only */
};

/* Orders items in reading order for a direction: left to right, top to bottom,
 * rows detected with a tolerance of half the smallest height. Returns indexes. */
std::vector<size_t> readingOrder(const std::vector<Frame> &frames, FlowDirection direction);

/* Lays out the frames inside `container`. Returned frames keep the input order. */
std::vector<Frame> flow(const std::vector<Frame> &frames, const Rect &container, const FlowSpec &spec);

/* ---- presets ---- */

enum class Preset {
	CornerTopLeft,
	CornerTopRight,
	CornerBottomLeft,
	CornerBottomRight,
	Centered,         /* 1 item: fit, centered */
	FullScreen,       /* 1 item: fill the canvas */
	LowerThird,       /* 1 item: bottom band on the left */
	PictureInPicture, /* 2 items: first fills, second in the bottom-right corner */
	SplitScreen,      /* 2 items side by side */
	OneAndTwo,        /* 3 items: one large on the left, two stacked on the right */
	GridTwoByTwo,     /* 4 items */
	VerticalStack,    /* 2 items: top half / bottom half (vertical canvases) */
};

struct PresetSpec {
	double margin = 48;       /* canvas pixels from the canvas edges */
	double gap = 24;          /* between items */
	double cornerScale = 0.3; /* width of a corner item, fraction of the canvas width */
};

struct PresetInfo {
	Preset preset;
	const char *key; /* stable id, also the localization key suffix */
	int minItems;
	int maxItems;
};

const std::vector<PresetInfo> &presets();
std::optional<Preset> presetFromKey(const std::string &key);

/* Target frames for the first `frames.size()` items (selection order). Items beyond
 * what the preset places keep their frame. Aspect ratios are kept (fit in cells),
 * except FullScreen and the background of picture in picture, which fill. */
std::vector<Frame> applyPreset(Preset preset, const std::vector<Frame> &frames, const Rect &canvas,
			       const PresetSpec &spec);

} // namespace layouts::core
