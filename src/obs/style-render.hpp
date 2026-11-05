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

/* Border and drop shadow of an item, following its rounded corners (filter
 * "layouts_border_shadow", kept last in the source's chain).
 *
 * Like the corners, the values belong to the scene: one set per scene token (see
 * corners.hpp), so the same camera can have a border in one scene and none in another.
 * Lengths are stored in canvas pixels together with the item's scale at the time they
 * were set; the filter converts them to source pixels while drawing. */

#include "core/style.hpp"

#include <obs.h>

namespace layouts::obs::style {

constexpr const char *kFilterId = "layouts_border_shadow";

void registerFilter();

/* Border and shadow of a scene item in its own scene (not the other scenes that show the
 * same source), as one undo step. Nothing left (no border, no shadow) removes the scene's
 * value, and the filter once no scene uses it. Groups are skipped. */
bool setBorderShadow(obs_sceneitem_t *item, const core::Style &style, bool undo = true);

/* Values of a scene item in its own scene (border and shadow fields only); false when it
 * has none. */
bool borderShadow(obs_sceneitem_t *item, core::Style &out);

/* Shadow values of a style after its preset (provisional): Soft = blur 24, offset (0, 8),
 * 35 %; Hard = blur 6, offset (0, 6), 60 %; Custom = the style's own fields; None = 0 %. */
void shadowValues(const core::Style &s, double &blur, double &x, double &y, double &opacity);

} // namespace layouts::obs::style
