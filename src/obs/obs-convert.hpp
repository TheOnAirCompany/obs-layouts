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

/* Conversions between libobs structures and the core model. */

#include "core/item-model.hpp"

#include <obs.h>
#include <graphics/matrix4.h>

namespace layouts {

inline core::ItemTransform fromObs(const obs_transform_info &info, const obs_sceneitem_crop &crop)
{
	core::ItemTransform t;
	t.pos = {info.pos.x, info.pos.y};
	t.scale = {info.scale.x, info.scale.y};
	t.rot = info.rot;
	t.align = info.alignment;
	t.boundsType = (core::BoundsType)info.bounds_type;
	t.boundsAlign = info.bounds_alignment;
	t.bounds = {info.bounds.x, info.bounds.y};
	t.cropToBounds = info.crop_to_bounds;
	t.crop = {crop.left, crop.top, crop.right, crop.bottom};
	return t;
}

inline void toObs(const core::ItemTransform &t, obs_transform_info &info, obs_sceneitem_crop &crop)
{
	vec2_set(&info.pos, (float)t.pos.x, (float)t.pos.y);
	vec2_set(&info.scale, (float)t.scale.x, (float)t.scale.y);
	info.rot = (float)t.rot;
	info.alignment = t.align;
	info.bounds_type = (enum obs_bounds_type)t.boundsType;
	info.bounds_alignment = t.boundsAlign;
	vec2_set(&info.bounds, (float)t.bounds.x, (float)t.bounds.y);
	info.crop_to_bounds = t.cropToBounds;
	crop.left = t.crop.left;
	crop.top = t.crop.top;
	crop.right = t.crop.right;
	crop.bottom = t.crop.bottom;
}

/* libobs matrices use row vectors (p' = p * M). */
inline core::Affine fromMatrix(const matrix4 &m)
{
	core::Affine a;
	a.a = m.x.x;
	a.b = m.x.y;
	a.c = m.y.x;
	a.d = m.y.y;
	a.tx = m.t.x;
	a.ty = m.t.y;
	return a;
}

} // namespace layouts
