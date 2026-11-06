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

/* Camera-only broadcast templates of 0.5 (16:9 canvas, landscapeOnly): portrait and
 * square tiles, host + strip, host + column, picture in picture, cinema bands...
 *
 * The geometry is drawn on a 1920 x 1080 reference and scaled to the canvas: fixed
 * insets and gaps scale with it, the gap of the shared splits follows spec.gap and the
 * content area follows spec.margin.
 *
 * Slot order: the host slot (Slot::host) comes first, then the guests in reading order.
 * When slots overlap (picture in picture, keys containing "Pip") the list is ordered back
 * to front: the first slot is at the back, the last one on top. applyBroadcast brings the
 * camera slots of a Pip template to the front one after the other in this order, so the
 * last slot ends up on top. Cam2HostPip is the one template where the host is not first:
 * the guest fills the canvas behind, the host is the inset (the host item is matched to
 * the slot flagged host, not to the first slot). */

#include "broadcast.hpp"

#include <string>
#include <vector>

namespace layouts::core {

/* Adds the camera templates, in display order. */
void appendCamTemplates(std::vector<BroadcastTemplate> &list);

/* Builds a camera template on a landscape canvas. False when the key is not one of them. */
bool buildCamTemplate(const std::string &key, const Rect &canvas, const BroadcastSpec &spec, std::vector<Slot> &out);

} // namespace layouts::core
