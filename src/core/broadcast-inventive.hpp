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

/* Inventive broadcast layouts (0.5): chat columns and tickers, bento grids, tilted cards,
 * Polaroids, bubbles, a diagonal split... Drawn on a 1920 x 1080 reference and scaled to
 * the canvas, so they are offered on landscape canvases only (landscapeOnly).
 *
 * Slot order is back to front: when slots overlap (a dock of bubbles over a full screen, a
 * stack of tilted cards, the front card of TiltedCards, a chat card over a full camera),
 * the later slot is drawn above the earlier one. Kinds are mixed in that order, so the
 * main guest is not always the first camera slot: it is the slot flagged `host`.
 *
 * Geometry is scaled per axis to the canvas (minus the spec margin); Round slots stay
 * square around their scaled center so bubbles remain circles. The spec gap is not used:
 * each template carries its own spacing. */

#include "broadcast.hpp"

#include <string>
#include <vector>

namespace layouts::core {

/* Appends the inventive templates, in display order. */
void appendInventiveTemplates(std::vector<BroadcastTemplate> &list);

/* Fills `out` with the slots of an inventive template on `canvas`. Returns false (and
 * leaves `out` untouched) when `key` is not an inventive template. */
bool buildInventiveTemplate(const std::string &key, const Rect &canvas, const BroadcastSpec &spec,
			    std::vector<Slot> &out);

} // namespace layouts::core
