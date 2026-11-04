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

/* Alignment, distribution and spacing on world-space bounding boxes.
 *
 * Every function returns one translation per input item, in input order. Locked
 * items always get a zero translation: they are never moved, but they still count
 * in the selection bounds (they are part of the selection). In distribution and
 * spacing they are left out of the set that is redistributed. */

#include "geometry.hpp"

#include <optional>
#include <vector>

namespace layouts::core {

struct ArrangeItem {
	Rect box;
	bool locked = false;
};

enum class AlignEdge { Left, HCenter, Right, Top, VCenter, Bottom };

enum class AlignTarget { Selection, Canvas, KeyObject };

Rect selectionBounds(const std::vector<ArrangeItem> &items);

/* Reference rectangle for an alignment, or nullopt if the target is not usable
 * (no items, key object index out of range). */
std::optional<Rect> alignReference(const std::vector<ArrangeItem> &items, AlignTarget target, const Rect &canvas,
				   int keyIndex);

std::vector<Vec2> align(const std::vector<ArrangeItem> &items, AlignEdge edge, const Rect &reference,
			int keyIndex = -1);

/* Evenly spaces item centers between the first and last item (by center) on an axis.
 * Needs at least 3 movable items. */
std::vector<Vec2> distributeCenters(const std::vector<ArrangeItem> &items, Axis axis);

/* Makes the gaps between consecutive items equal, first and last item stay.
 * Needs at least 3 movable items. */
std::vector<Vec2> distributeSpacing(const std::vector<ArrangeItem> &items, Axis axis);

/* Places items one after another with an exact gap, in spatial order, starting from
 * the first item (smallest start edge), which stays. Needs at least 2 movable items. */
std::vector<Vec2> spaceExactly(const std::vector<ArrangeItem> &items, Axis axis, double gap);

int movableCount(const std::vector<ArrangeItem> &items);

/* Current gap between consecutive items along an axis if all gaps are equal
 * (within eps), used to show the spacing field value. */
std::optional<double> uniformGap(const std::vector<ArrangeItem> &items, Axis axis, double eps = 0.5);

} // namespace layouts::core
