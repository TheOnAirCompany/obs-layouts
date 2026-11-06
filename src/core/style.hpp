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

/* Item style (0.5): corners, border and shadow. Defaults live per source type
 * (store, "styles.<role>"), an item may override them (private setting
 * "layouts_style", same JSON as toJson). Values approved on the 0.5 design review. */

#include "geometry.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace layouts::core {

enum class RadiusMode { Auto, Pixels, Round, None };
enum class ShadowPreset { None, Soft, Hard, Custom };

struct Style {
	RadiusMode radiusMode = RadiusMode::Auto; /* Auto = 4 % of the short side, 6..48 px */
	double radiusPx = 0;
	bool border = false;
	double borderWidth = 4;
	uint32_t borderColor = 0xFFFFFFFF; /* ARGB */
	ShadowPreset shadow = ShadowPreset::None;
	double shadowBlur = 16, shadowOffsetX = 0, shadowOffsetY = 6,
	       shadowOpacity = 0.25; /* Custom starts from Soft */
	uint32_t shadowColor = 0xFF000000;
	bool fadeShadowInTransition = true;
	/* Diagonal mask of a Polygon slot: corners in cell-normalized coordinates (0..1,
	 * clockwise). Empty = no mask. JSON key "polygon": [[x, y], ...]. */
	std::vector<Vec2> polygon;
};

/* Radius in canvas pixels for a visible size w x h (never more than half the short side). */
double styleRadius(const Style &s, double w, double h);

/* Compact JSON object. fromJson starts from the defaults: missing keys keep them and
 * unknown keys are ignored. Returns false (out untouched) on malformed JSON. */
std::string toJson(const Style &s);
bool fromJson(const std::string &json, Style &out);

} // namespace layouts::core
