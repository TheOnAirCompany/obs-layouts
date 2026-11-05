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

/* Rounded corners: a filter registered by Layouts ("Rounded corners") that masks the
 * source with anti-aliased rounded corners. The mask follows the crop of the scene item
 * being drawn, so a cropped camera keeps its corners. The radius is stored on the source
 * in source pixels; the Inspector shows and edits it in canvas pixels. */

#include "core/expression.hpp"

namespace layouts {

struct ItemEntry;

namespace corners {

constexpr const char *kFilterId = "layouts_rounded_corners";

void registerFilter();

/* Radius of an item in canvas pixels, 0 when its corners are square. */
double radius(const ItemEntry &e);

/* Sets the radius (canvas pixels) of the selected items as one undo step. 0 removes the
 * filter. Groups are skipped. */
bool setRadius(const core::Expression &expr);

/* Field label drag: applied live, one undo step once finished. */
void scrub(double delta, bool finished);

} // namespace corners
} // namespace layouts
