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

/* Broadcast layouts: the multi-guest arrangements used on TV and in live shows
 * (duo, three columns, 1 + 2, grids, screen + guests...), generated for a number of
 * cameras and screens.
 *
 * Each template is a list of slots. A camera slot is filled edge to edge by cropping
 * the camera (like TV splits; the crop keeps the top of the frame where faces are). A
 * screen slot always shows the whole content (fit, never cropped). */

#include "geometry.hpp"
#include "item-model.hpp"

#include <string>
#include <vector>

namespace layouts::core {

enum class SlotKind { Camera, Screen, Chat };

/* How a slot is cut out (0.5). Round = radius 50 % of the short side (bubbles, pills).
 * Polygon = diagonal mask, `polygon` holds its corners in cell-normalized coordinates
 * (0..1, clockwise). */
enum class SlotShape { Rect, Round, Polygon };

struct Slot {
	SlotKind kind;
	Rect cell;
	double rotation = 0; /* degrees, clockwise, around the cell center */
	SlotShape shape = SlotShape::Rect;
	std::vector<Vec2> polygon;
	bool frame = false;  /* the template wants a border (Polaroid) */
	bool ticker = false; /* chat shown as a horizontal band */
	bool host = false;   /* the main guest of the layout (Host role first) */
};

struct BroadcastSpec {
	double gap = 8;    /* between tiles */
	double margin = 0; /* around the layout (0 = full bleed, like TV) */
};

struct BroadcastTemplate {
	std::string key; /* stable id, also the localization key suffix */
	int cameras;
	int screens;
	int chats = 0;
	bool landscapeOnly = false; /* 0.5 templates are drawn for 16:9 only */
};

/* Every template, in display order. */
const std::vector<BroadcastTemplate> &broadcastTemplates();

/* Templates that use exactly this number of cameras and screens. */
std::vector<BroadcastTemplate> templatesFor(int cameras, int screens);

/* Slots of a template on a canvas. Camera slots come first, in reading order, then
 * screen slots. Unknown keys give no slot. On a portrait canvas (taller than wide) the
 * guests stack instead of sitting side by side and screens span the width at the top. */
std::vector<Slot> buildBroadcast(const std::string &key, const Rect &canvas, const BroadcastSpec &spec);

/* Crop (source pixels) and scale so that a source of srcW x srcH exactly fills a cell
 * of cellW x cellH. Horizontal crops are centered; vertical crops keep `keepTop` of the
 * removed height at the bottom (0.3 = 30 % from the top, 70 % from the bottom). */
struct CoverResult {
	Crop crop;
	Vec2 scale;
};
CoverResult coverCell(uint32_t srcW, uint32_t srcH, double cellW, double cellH, double keepTop = 0.3);

} // namespace layouts::core
