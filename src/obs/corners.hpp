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
 * being drawn, so a cropped camera keeps its corners.
 *
 * The radius belongs to the scene: the filter keeps one value per scene (in source
 * pixels), and a small filter on each such scene ("Rounded corners: scene") tells it which
 * scene is rendering. The same camera can be round in one scene and square in another.
 * The Inspector shows and edits the radius in canvas pixels. */

#include "core/expression.hpp"
#include "core/geometry.hpp"
#include "core/style.hpp"

#include <obs.h>

#include <vector>

namespace layouts {

struct ItemEntry;

namespace corners {

constexpr const char *kFilterId = "layouts_rounded_corners";
constexpr const char *kSceneFilterId = "layouts_scene_corners";

void registerFilter();

/* Remembers the transition on the program (called on OBS events; self-test after setting
 * the output channel itself). */
void watchProgram();

/* Radius of an item in canvas pixels, 0 when its corners are square. */
double radius(const ItemEntry &e);

/* Sets the radius (canvas pixels) of the selected items in the edited scene only, as one
 * undo step. The filter goes away when no scene has corners left. Groups are skipped. */
bool setRadius(const core::Expression &expr);

/* Field label drag: applied live, one undo step once finished. */
void scrub(double delta, bool finished);

/* Per-item style (0.5).
 *
 * How an item's style reaches the filter: the filter only knows its source and, while a
 * scene renders, that scene's token; OBS never tells a filter which scene item draws it.
 * So the style is copied, per scene, into the source's filter settings ("scenes" entries:
 * mode, radius, ratio, scale_x/scale_y, polygon), and the item's private setting
 * "layouts_style" stays the reference. The UI calls applyItemStyle whenever
 * "layouts_style" or the item's size changes (the Auto/Round radius of a cropped item uses
 * the scale stored here; an uncropped one reads its scale from the render matrix).
 * Limit: two items of the same source in the same scene share one shape (the last
 * applied).
 *
 * applyItemStyle ensures the filter (and the scene marker) exist, stores the item's
 * radius mode and its mask polygon (cell-normalized, convex, up to 8 points; read from
 * "layouts_style".polygon when polygon is null) for the item's scene (its parent scene
 * for an item of a group), as one undo step. Returns false for groups and scenes. */
bool applyItemStyle(obs_sceneitem_t *item, const core::Style &style, const std::vector<core::Vec2> *polygon = nullptr);

/* The "polygon" of an item's "layouts_style", made convex (empty: none). */
std::vector<core::Vec2> itemPolygon(obs_sceneitem_t *item);

} // namespace corners
} // namespace layouts
