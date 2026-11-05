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

#include "swap.hpp"

#include <cmath>

namespace layouts::core {

SlotRect slotOf(const ItemModel &m, const Affine &parent)
{
	const VisualFrame f = visualFrame(m, parent);
	return {f.center(), f.width, f.height, f.rotation};
}

std::optional<SlotKind> swapKind(Role role)
{
	switch (role) {
	case Role::Host:
	case Role::Camera:
		return SlotKind::Camera;
	case Role::Screen:
		return SlotKind::Screen;
	default:
		return std::nullopt;
	}
}

bool canSwap(Role a, Role b)
{
	const std::optional<SlotKind> ka = swapKind(a), kb = swapKind(b);
	return ka && kb && *ka == *kb;
}

ItemTransform placeInSlot(const ItemModel &m, const Affine &parent, const SlotRect &slot, FrameMode mode,
			  double keepTop)
{
	if (m.sourceWidth == 0 || m.sourceHeight == 0 || !(slot.width > 0) || !(slot.height > 0))
		return m.t;
	const RefPoint center{0.5, 0.5};

	/* Start from the whole source, no bounds, in the slot's rotation. */
	ItemModel w = m;
	w.t.crop = Crop();
	w.t.cropToBounds = false;
	if (w.t.boundsType != BoundsType::None) {
		const ItemGeometry g = computeGeometry(m);
		const double sx = std::fabs(g.boxScale.x) / (double)m.sourceWidth;
		const double sy = std::fabs(g.boxScale.y) / (double)m.sourceHeight;
		w.t.boundsType = BoundsType::None;
		w.t.bounds = {};
		w.t.scale = {std::copysign(sx > 0 ? sx : 1.0, m.t.scale.x),
			     std::copysign(sy > 0 ? sy : 1.0, m.t.scale.y)};
	}
	w.t.rot = normalizeDegrees(slot.rotation - parent.rotationDeg());

	if (mode == FrameMode::Stretch)
		w.t = setWorldSize(w, parent, slot.width, slot.height, center);
	else
		/* The anchor picks the part of the picture kept: centered across, keepTop down. */
		w.t = setFrame(w, parent, slot.width, slot.height, mode,
			       mode == FrameMode::Fill ? RefPoint{0.5, keepTop} : center);
	return moveRefTo(w, parent, center, slot.center);
}

SwapResult swapSlots(const ItemModel &a, const Affine &parentA, FrameMode modeA, const ItemModel &b,
		     const Affine &parentB, FrameMode modeB, double keepTop)
{
	const SlotRect sa = slotOf(a, parentA), sb = slotOf(b, parentB);
	return {placeInSlot(a, parentA, sb, modeA, keepTop), placeInSlot(b, parentB, sa, modeB, keepTop)};
}

} // namespace layouts::core
