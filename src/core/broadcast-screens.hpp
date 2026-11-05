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

/* 0.5 screen templates: screens alone, and screens with cameras (42 layouts, 16:9 only).
 *
 * Each template is drawn on a 1920 x 1080 design board, then scaled uniformly into the
 * canvas (minus the margin) and centered, so squares, bubbles and 9:16 cameras keep their
 * shape. Where the design uses the standard gap, the spec gap is used instead (converted
 * to design units); the other spacings are part of the design and scale with it.
 *
 * Slot order follows the rest of the engine: cameras first (the host first, then the
 * guests in the order of the design), then screens. Some templates overlap on purpose
 * (picture in picture, bubble, a screen floating over a camera): see overlayOrder() for
 * the stacking they need. */

#include "broadcast.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace layouts::core {

/* Adds the screen templates at the end of the list (landscapeOnly = true). */
void appendScreenTemplates(std::vector<BroadcastTemplate> &list);

/* Builds a screen template. Returns false (and leaves `out` alone) for other keys. */
bool buildScreenTemplate(const std::string &key, const Rect &canvas, const BroadcastSpec &spec,
			 std::vector<Slot> &out);

/* Stacking of overlapping cells: the indices of the cells drawn over another one (a cell
 * that overlaps a larger cell: PiP camera, bubble, floating screen), largest first. Bring
 * each of them to the front in this order and the smallest ends on top. Cells that overlap
 * nothing are not listed and keep their place. */
std::vector<size_t> overlayOrder(const std::vector<Rect> &cells);

} // namespace layouts::core
