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

/* Swap (0.5): two items exchange their slots. A slot is the visible cell of an item:
 * center, size and rotation on the canvas. Each item keeps its own flips; its crop is
 * recomputed from the whole source so that it covers the new cell (cameras, cropped on
 * the face like coverCell) or fits in it (screens, chats), following its frame mode.
 * Z-order and the per-item style stay with the slot too; that part lives in the OBS
 * layer (commands::swapItems). */

#include "broadcast.hpp"
#include "item-model.hpp"
#include "roles.hpp"

#include <optional>

namespace layouts::core {

struct SlotRect {
	Vec2 center;
	double width = 0;
	double height = 0;
	double rotation = 0; /* degrees, world */
};

/* Visible cell of an item, in world (canvas) space. */
SlotRect slotOf(const ItemModel &m, const Affine &parent = Affine::identity());

/* Kind of slot a role can be swapped into: Host and Camera share camera slots, Screen
 * goes with Screen. Background, Graphics and None never swap. */
std::optional<SlotKind> swapKind(Role role);
bool canSwap(Role a, Role b);

/* Transform that puts the item in this slot. mode Fill covers the cell (the crop of the
 * whole source is recomputed, keepTop of the removed height taken from the top), Fit
 * shows the whole source inside the cell, Stretch distorts it to the cell. Flips are
 * kept, the rotation becomes the slot's. */
ItemTransform placeInSlot(const ItemModel &m, const Affine &parent, const SlotRect &slot, FrameMode mode,
			  double keepTop = 0.3);

struct SwapResult {
	ItemTransform a; /* a in the slot of b */
	ItemTransform b; /* b in the slot of a */
};

SwapResult swapSlots(const ItemModel &a, const Affine &parentA, FrameMode modeA, const ItemModel &b,
		     const Affine &parentB, FrameMode modeB, double keepTop = 0.3);

} // namespace layouts::core
