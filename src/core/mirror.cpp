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

#include "mirror.hpp"

#include <algorithm>

namespace layouts::core {

std::vector<Slot> mirrorSlots(const std::vector<Slot> &in, const Rect &canvas, bool h, bool v)
{
	std::vector<Slot> out = in;
	if (!h && !v)
		return out;
	for (Slot &s : out) {
		if (h)
			s.cell.x = canvas.x + (canvas.x + canvas.w) - (s.cell.x + s.cell.w);
		if (v)
			s.cell.y = canvas.y + (canvas.y + canvas.h) - (s.cell.y + s.cell.h);
		/* Both axes = a half turn: the tilt keeps its direction. */
		if (h != v && s.rotation != 0)
			s.rotation = -s.rotation;
		for (Vec2 &p : s.polygon) {
			if (h)
				p.x = 1.0 - p.x;
			if (v)
				p.y = 1.0 - p.y;
		}
		/* A single mirror turns a clockwise outline counterclockwise. */
		if (h != v)
			std::reverse(s.polygon.begin(), s.polygon.end());
	}
	return out;
}

} // namespace layouts::core
