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

#include "morph.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace layouts::core {

namespace {

double lerp(double a, double b, double t)
{
	return a + (b - a) * t;
}

double dot(const Vec2 &a, const Vec2 &b)
{
	return a.x * b.x + a.y * b.y;
}

bool near(double a, double b)
{
	return std::fabs(a - b) <= 1e-6;
}

bool sameTransform(const ItemTransform &a, const ItemTransform &b)
{
	return near(a.pos.x, b.pos.x) && near(a.pos.y, b.pos.y) && near(a.scale.x, b.scale.x) &&
	       near(a.scale.y, b.scale.y) && near(a.rot, b.rot) && a.align == b.align && a.boundsType == b.boundsType &&
	       a.boundsAlign == b.boundsAlign && near(a.bounds.x, b.bounds.x) && near(a.bounds.y, b.bounds.y) &&
	       a.cropToBounds == b.cropToBounds && a.crop == b.crop;
}

} // namespace

double easeInOutCubic(double t)
{
	t = std::clamp(t, 0.0, 1.0);
	return t < 0.5 ? 4.0 * t * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 3.0) / 2.0;
}

bool hasVisualState(const ItemModel &m)
{
	const int cw = (int)m.sourceWidth - m.t.crop.left - m.t.crop.right;
	const int ch = (int)m.sourceHeight - m.t.crop.top - m.t.crop.bottom;
	return m.sourceWidth > 0 && m.sourceHeight > 0 && cw > 0 && ch > 0;
}

VisualState visualState(const ItemModel &m)
{
	VisualState v;
	v.sourceWidth = m.sourceWidth;
	v.sourceHeight = m.sourceHeight;
	v.cropLeft = m.t.crop.left;
	v.cropTop = m.t.crop.top;
	v.cropRight = m.t.crop.right;
	v.cropBottom = m.t.crop.bottom;
	v.rotation = m.t.rot;
	if (!hasVisualState(m)) {
		v.center = m.t.pos;
		return v;
	}

	const ItemGeometry g = computeGeometry(m);
	const double cw = g.croppedWidth, ch = g.croppedHeight;

	/* Shown part of the cropped source, in cropped source pixels. */
	double x0 = 0.0, y0 = 0.0, x1 = cw, y1 = ch;
	if (m.t.cropToBounds && m.t.boundsType != BoundsType::None && g.draw.invertible()) {
		const Affine toLocal = g.draw.inverted();
		const Vec2 corners[4] = {toLocal.map(g.box.map({0, 0})), toLocal.map(g.box.map({1, 0})),
					 toLocal.map(g.box.map({1, 1})), toLocal.map(g.box.map({0, 1}))};
		double bx0 = corners[0].x, bx1 = corners[0].x, by0 = corners[0].y, by1 = corners[0].y;
		for (const Vec2 &c : corners) {
			bx0 = std::min(bx0, c.x);
			bx1 = std::max(bx1, c.x);
			by0 = std::min(by0, c.y);
			by1 = std::max(by1, c.y);
		}
		x0 = std::clamp(bx0, 0.0, cw);
		x1 = std::clamp(bx1, x0, cw);
		y0 = std::clamp(by0, 0.0, ch);
		y1 = std::clamp(by1, y0, ch);
		v.cropLeft += x0;
		v.cropRight += cw - x1;
		v.cropTop += y0;
		v.cropBottom += ch - y1;
	}

	v.center = g.draw.map({(x0 + x1) / 2.0, (y0 + y1) / 2.0});
	const Vec2 ex = g.draw.mapVector({x1 - x0, 0.0});
	const Vec2 ey = g.draw.mapVector({0.0, y1 - y0});
	v.width = ex.length();
	v.height = ey.length();
	const Affine r = Affine::rotationDeg(m.t.rot);
	v.flipX = dot(ex, r.mapVector({1, 0})) < 0.0;
	v.flipY = dot(ey, r.mapVector({0, 1})) < 0.0;
	return v;
}

ItemTransform transformFor(const VisualState &v)
{
	ItemTransform t;
	t.pos = v.center;
	t.rot = v.rotation;
	t.align = kAlignCenter;
	t.boundsType = BoundsType::None;

	const int sw = (int)v.sourceWidth, sh = (int)v.sourceHeight;
	int l = std::max(0, (int)std::lround(v.cropLeft));
	int r = std::max(0, (int)std::lround(v.cropRight));
	int tp = std::max(0, (int)std::lround(v.cropTop));
	int b = std::max(0, (int)std::lround(v.cropBottom));
	if (sw > 0 && l + r >= sw)
		r = std::max(0, sw - l - 1), l = std::min(l, sw - 1);
	if (sh > 0 && tp + b >= sh)
		b = std::max(0, sh - tp - 1), tp = std::min(tp, sh - 1);
	t.crop = {l, tp, r, b};

	const double cw = std::max(1, sw - l - r), ch = std::max(1, sh - tp - b);
	/* A zero scale is legal in OBS but makes the matrices singular. */
	const double w = std::max(v.width, 1e-4), h = std::max(v.height, 1e-4);
	t.scale = {(v.flipX ? -1.0 : 1.0) * w / cw, (v.flipY ? -1.0 : 1.0) * h / ch};

	/* OBS moves the origin of flipped items: the position is solved on the real
	 * geometry (pos only translates the result). */
	if (sw > 0 && sh > 0) {
		ItemModel probe;
		probe.sourceWidth = v.sourceWidth;
		probe.sourceHeight = v.sourceHeight;
		probe.t = t;
		probe.t.pos = {0.0, 0.0};
		const ItemGeometry g = computeGeometry(probe);
		const Vec2 c = g.draw.map({g.croppedWidth / 2.0, g.croppedHeight / 2.0});
		t.pos = v.center - c;
	}
	return t;
}

VisualState interpolate(const VisualState &a, const VisualState &b, double t)
{
	VisualState v = b;
	v.center = {lerp(a.center.x, b.center.x, t), lerp(a.center.y, b.center.y, t)};

	/* Signed sizes: a flip that changes passes through zero. */
	const double sw = lerp(a.flipX ? -a.width : a.width, b.flipX ? -b.width : b.width, t);
	const double sh = lerp(a.flipY ? -a.height : a.height, b.flipY ? -b.height : b.height, t);
	v.width = std::fabs(sw);
	v.height = std::fabs(sh);
	v.flipX = sw < 0.0 || (sw == 0.0 && b.flipX);
	v.flipY = sh < 0.0 || (sh == 0.0 && b.flipY);

	v.rotation = a.rotation + normalizeDegrees(b.rotation - a.rotation) * t;

	if (a.sourceWidth == b.sourceWidth && a.sourceHeight == b.sourceHeight) {
		v.cropLeft = lerp(a.cropLeft, b.cropLeft, t);
		v.cropTop = lerp(a.cropTop, b.cropTop, t);
		v.cropRight = lerp(a.cropRight, b.cropRight, t);
		v.cropBottom = lerp(a.cropBottom, b.cropBottom, t);
	}
	return v;
}

ItemTransform interpolate(const ItemModel &a, const ItemModel &b, double t)
{
	if (t <= 0.0)
		return a.t;
	if (t >= 1.0)
		return b.t;
	if (hasVisualState(a) && hasVisualState(b))
		return transformFor(interpolate(visualState(a), visualState(b), t));

	/* No source size yet: plain lerp of the fields, the rest from the target. */
	ItemTransform r = b.t;
	r.pos = {lerp(a.t.pos.x, b.t.pos.x, t), lerp(a.t.pos.y, b.t.pos.y, t)};
	r.scale = {lerp(a.t.scale.x, b.t.scale.x, t), lerp(a.t.scale.y, b.t.scale.y, t)};
	r.bounds = {lerp(a.t.bounds.x, b.t.bounds.x, t), lerp(a.t.bounds.y, b.t.bounds.y, t)};
	r.rot = a.t.rot + normalizeDegrees(b.t.rot - a.t.rot) * t;
	r.crop = {(int)std::lround(lerp(a.t.crop.left, b.t.crop.left, t)),
		  (int)std::lround(lerp(a.t.crop.top, b.t.crop.top, t)),
		  (int)std::lround(lerp(a.t.crop.right, b.t.crop.right, t)),
		  (int)std::lround(lerp(a.t.crop.bottom, b.t.crop.bottom, t))};
	return r;
}

MorphPlan planMorph(const std::vector<MorphItemState> &current, const std::vector<MorphItemState> &targets)
{
	std::unordered_map<int64_t, const MorphItemState *> byId;
	for (const MorphItemState &c : current)
		byId[c.id] = &c;

	MorphPlan plan;
	for (const MorphItemState &target : targets) {
		const auto it = byId.find(target.id);
		const MorphItemState *cur = it == byId.end() ? nullptr : it->second;
		const bool wasVisible = cur && cur->visible;
		const double opacity = wasVisible ? std::clamp(cur->opacity, 0.0, 1.0) : 0.0;

		MorphTrack track;
		track.id = target.id;
		track.to = target.model;
		track.from = cur ? cur->model : target.model;
		track.opacityFrom = opacity;
		if (target.visible && wasVisible) {
			track.kind = TrackKind::Move;
			track.opacityTo = 1.0;
			if (sameTransform(track.from.t, track.to.t) && opacity >= 1.0)
				continue;
		} else if (target.visible) {
			track.kind = TrackKind::Enter;
			track.from = target.model;
			track.opacityTo = 1.0;
		} else if (wasVisible) {
			track.kind = TrackKind::Leave;
			track.opacityTo = 0.0;
		} else {
			continue;
		}
		plan.tracks.push_back(track);
	}
	return plan;
}

std::vector<MorphFrameItem> sampleMorph(const MorphPlan &plan, double progress)
{
	progress = std::clamp(progress, 0.0, 1.0);
	const double e = progress >= 1.0 ? 1.0 : easeInOutCubic(progress);

	std::vector<MorphFrameItem> frame;
	frame.reserve(plan.tracks.size());
	for (const MorphTrack &track : plan.tracks) {
		MorphFrameItem f;
		f.id = track.id;
		f.opacity = lerp(track.opacityFrom, track.opacityTo, e);
		switch (track.kind) {
		case TrackKind::Move:
			f.t = interpolate(track.from, track.to, e);
			break;
		case TrackKind::Enter:
			f.t = track.to.t;
			break;
		case TrackKind::Leave:
			f.t = progress >= 1.0 ? track.to.t : track.from.t;
			break;
		}
		frame.push_back(f);
	}
	return frame;
}

} // namespace layouts::core
