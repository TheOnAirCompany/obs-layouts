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

/* Mirrored broadcast layouts: the same template with the guests on the other side
 * ("screen on the right, camera on the left" becomes the opposite). */

#include "broadcast.hpp"

#include <vector>

namespace layouts::core {

/* Mirrors the slots across the vertical axis of the canvas (h) and/or its horizontal
 * axis (v). Cells move to the other side, rotations are negated when the picture is
 * mirrored on a single axis, polygons are mirrored inside their cell (corners kept
 * clockwise). The slot order is kept, so the assignment of sources does not change. */
std::vector<Slot> mirrorSlots(const std::vector<Slot> &in, const Rect &canvas, bool h, bool v);

} // namespace layouts::core
