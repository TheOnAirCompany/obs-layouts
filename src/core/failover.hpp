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

/* Live failover: when a camera or a screen drops on program, the layout falls back to a
 * template with one tile less, then comes back to the original one when the source
 * returns. This file only chooses the fallback (pure, no OBS). */

#include <set>
#include <string>
#include <vector>

namespace layouts::core {

/* Source name of each slot of a template, in slot order (cameras, then screens, then
 * chats, as buildBroadcast returns them). An empty name is an empty slot. */
using SlotAssignment = std::vector<std::string>;

struct FailoverChoice {
	bool ok = false; /* false: no template fits what is left, keep the layout as it is */
	std::string key;
	SlotAssignment assignment;
};

/* Family of a template: its key with every number replaced by '#'
 * ("Screen1Cam3Strip" -> "Screen#Cam#Strip"). */
std::string templateFamily(const std::string &key);

/* Template to use while the `lost` sources are missing from `key` filled with
 * `assignment`. The lost sources leave their slots and the others keep their order, so
 * the host stays in the host slot. Preference: a template of the same family with the
 * same number of chats, then the first template (display order) with the remaining
 * cameras and screens. Portrait canvases skip landscape-only templates. When nothing is
 * lost, the original key and assignment come back unchanged (that is the restore). */
FailoverChoice chooseFallback(const std::string &key, const SlotAssignment &assignment,
			      const std::set<std::string> &lost, bool portrait = false);

} // namespace layouts::core
