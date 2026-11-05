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

/* Applying a broadcast template (0.5): which source goes in which slot, in which
 * order they stack, how a rotated slot is placed and what style a slot asks for. */

#include "broadcast.hpp"
#include "item-model.hpp"
#include "style.hpp"

#include <vector>

namespace layouts::core {

/* The same item turned by `deg` (clockwise) around `center`, a point of its parent
 * space: what it shows, its crop and its flips are unchanged. Works whatever the
 * alignment, since OBS turns an item around its position. */
ItemTransform rotateAround(const ItemTransform &t, const Vec2 &center, double deg);

/* Which item goes in each slot (-1 = the slot stays empty). kinds[i] is the slot kind
 * item i can fill, hosts[i] whether it has the Host role. Host slots are served first
 * and take a host when there is one; the other slots then take the remaining items in
 * the order given. */
std::vector<int> assignItems(const std::vector<Slot> &slots, const std::vector<SlotKind> &kinds,
			     const std::vector<bool> &hosts);

/* Slot indices back to front. Slots stack in template order (the first one at the
 * back), except that a slot lying inside a later, larger one (an inset camera) always
 * goes in front of it. */
std::vector<size_t> stackOrder(const std::vector<Slot> &slots);

/* Adds what the slot asks for to `style` (Round: round corners, Polygon: the mask,
 * frame: a border). Returns false, style untouched, for a plain slot. */
bool applySlotStyle(const Slot &slot, Style &style);

/* Removes what applySlotStyle may have added, for an item that moves to a plain slot.
 * Returns true when what is left is the default style (the override can go). */
bool clearSlotStyle(Style &style);

} // namespace layouts::core
