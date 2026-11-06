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

#include "slot-apply.hpp"

#include <cmath>

namespace layouts::core {

ItemTransform rotateAround(const ItemTransform &t, const Vec2 &center, double deg)
{
	if (deg == 0)
		return t;
	/* OBS draws an item as translate(pos) * rotate(rot) * (scale, crop, alignment):
	 * turning it around `center` moves pos around center and adds to rot. */
	ItemTransform out = t;
	out.pos = center + Affine::rotationDeg(deg).mapVector(t.pos - center);
	double rot = std::fmod(t.rot + deg, 360.0);
	if (rot < 0)
		rot += 360.0;
	out.rot = rot;
	return out;
}

std::vector<int> assignItems(const std::vector<Slot> &slots, const std::vector<SlotKind> &kinds,
			     const std::vector<bool> &hosts)
{
	std::vector<int> out(slots.size(), -1);
	std::vector<bool> used(kinds.size(), false);
	auto take = [&](size_t s, bool hostOnly) {
		for (size_t i = 0; i < kinds.size(); i++) {
			const bool host = i < hosts.size() && hosts[i];
			if (!used[i] && kinds[i] == slots[s].kind && (!hostOnly || host)) {
				used[i] = true;
				out[s] = (int)i;
				return;
			}
		}
	};
	for (size_t s = 0; s < slots.size(); s++) {
		if (slots[s].host)
			take(s, true);
	}
	for (size_t s = 0; s < slots.size(); s++) {
		if (out[s] < 0)
			take(s, false);
	}
	return out;
}

namespace {

bool insideLarger(const Rect &inner, const Rect &outer)
{
	const double e = 1e-6;
	const bool inside = inner.x >= outer.x - e && inner.y >= outer.y - e && inner.right() <= outer.right() + e &&
			    inner.bottom() <= outer.bottom() + e;
	return inside && inner.w * inner.h < outer.w * outer.h - e;
}

} // namespace

std::vector<size_t> stackOrder(const std::vector<Slot> &slots)
{
	std::vector<size_t> order;
	std::vector<bool> done(slots.size(), false);
	/* Stable topological order: the first remaining slot that no remaining slot must
	 * cover (strict containment has no cycle). */
	while (order.size() < slots.size()) {
		size_t pick = slots.size();
		for (size_t k = 0; k < slots.size() && pick == slots.size(); k++) {
			if (done[k])
				continue;
			bool blocked = false;
			for (size_t m = 0; m < slots.size() && !blocked; m++)
				blocked = !done[m] && m != k && insideLarger(slots[k].cell, slots[m].cell);
			if (!blocked)
				pick = k;
		}
		if (pick == slots.size()) /* unreachable, kept for safety */
			for (size_t k = 0; k < slots.size(); k++)
				if (!done[k]) {
					pick = k;
					break;
				}
		done[pick] = true;
		order.push_back(pick);
	}
	return order;
}

bool applySlotStyle(const Slot &slot, Style &style)
{
	bool changed = false;
	if (slot.shape == SlotShape::Round) {
		style.radiusMode = RadiusMode::Round;
		style.polygon.clear();
		changed = true;
	} else if (slot.shape == SlotShape::Polygon && slot.polygon.size() >= 3) {
		style.polygon = slot.polygon;
		/* The mask has sharp corners: rounding them would cut inside the polygon. */
		style.radiusMode = RadiusMode::None;
		changed = true;
	}
	if (slot.frame) {
		style.border = true;
		changed = true;
	}
	return changed;
}

bool clearSlotStyle(Style &style)
{
	const Style def;
	if (style.radiusMode == RadiusMode::Round || (!style.polygon.empty() && style.radiusMode == RadiusMode::None))
		style.radiusMode = def.radiusMode;
	style.polygon.clear();
	style.border = false;
	return style.radiusMode == def.radiusMode && style.radiusPx == def.radiusPx && style.border == def.border &&
	       style.borderWidth == def.borderWidth && style.borderColor == def.borderColor &&
	       style.shadow == def.shadow && style.shadowBlur == def.shadowBlur &&
	       style.shadowOffsetX == def.shadowOffsetX && style.shadowOffsetY == def.shadowOffsetY &&
	       style.shadowOpacity == def.shadowOpacity && style.shadowColor == def.shadowColor &&
	       style.fadeShadowInTransition == def.fadeShadowInTransition;
}

} // namespace layouts::core
