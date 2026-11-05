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

#include "item-model.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>

namespace layouts::core {

namespace {

/* libobs converts possibly negative floats to uint32_t, which is undefined in C.
 * Reproduce what the shipping compilers emit so our geometry matches OBS bit for bit:
 * arm64 saturates to 0, x86-64 goes through a 64-bit signed conversion and wraps. */
uint32_t obsFloatToU32(float f)
{
	if (!std::isfinite(f))
		return 0;
#if defined(__aarch64__) || defined(_M_ARM64)
	if (f <= 0.0f)
		return 0;
	if (f >= 4294967295.0f)
		return UINT32_MAX;
	return (uint32_t)f;
#else
	return (uint32_t)(int64_t)f;
#endif
}

void addAlignment(float &vx, float &vy, uint32_t align, int cx, int cy)
{
	if (align & kAlignRight)
		vx += (float)cx;
	else if ((align & kAlignLeft) == 0)
		vx += (float)(cx / 2);

	if (align & kAlignBottom)
		vy += (float)cy;
	else if ((align & kAlignTop) == 0)
		vy += (float)(cy / 2);
}

bool cropToBoundsActive(const ItemTransform &t)
{
	return t.cropToBounds && (t.boundsType == BoundsType::ScaleOuter || t.boundsType == BoundsType::ScaleToHeight ||
				  t.boundsType == BoundsType::ScaleToWidth);
}

/* Mirror of calculate_bounds_data() (obs-scene.c). */
void calculateBounds(const ItemTransform &t, float &ox, float &oy, float &sx, float &sy, uint32_t &cx, uint32_t &cy)
{
	const float bx = (float)t.bounds.x;
	const float by = (float)t.bounds.y;

	float width = (float)cx * std::fabs(sx);
	float height = (float)cy * std::fabs(sy);
	const float itemAspect = width / height;
	const float boundsAspect = bx / by;
	BoundsType type = t.boundsType;

	if (t.boundsType == BoundsType::MaxOnly && (width > bx || height > by))
		type = BoundsType::ScaleInner;

	if (type == BoundsType::ScaleInner || type == BoundsType::ScaleOuter) {
		bool useWidth = boundsAspect < itemAspect;
		if (t.boundsType == BoundsType::ScaleOuter)
			useWidth = !useWidth;
		const float mul = useWidth ? bx / width : by / height;
		sx *= mul;
		sy *= mul;
	} else if (type == BoundsType::ScaleToWidth) {
		const float mul = bx / width;
		sx *= mul;
		sy *= mul;
	} else if (type == BoundsType::ScaleToHeight) {
		const float mul = by / height;
		sx *= mul;
		sy *= mul;
	} else if (type == BoundsType::Stretch) {
		sx = std::copysign(bx / (float)cx, sx);
		sy = std::copysign(by / (float)cy, sy);
	}

	width = (float)cx * sx;
	height = (float)cy * sy;

	const float widthDiff = bx - std::fabs(width);
	const float heightDiff = by - std::fabs(height);
	cx = obsFloatToU32(bx);
	cy = obsFloatToU32(by);

	addAlignment(ox, oy, t.boundsAlign, (int)-widthDiff, (int)-heightDiff);

	if (cropToBoundsActive(t) && (widthDiff < -0.1f || heightDiff < -0.1f)) {
		const bool cropWidth = widthDiff < -0.1f;
		const bool cropFlipped = cropWidth ? width < 0.0f : height < 0.0f;
		const float cropDiff = cropWidth ? widthDiff : heightDiff;
		float cropOrigin = cropWidth ? ox : oy;
		const uint32_t mask = cropWidth ? (kAlignLeft | kAlignRight) : (kAlignTop | kAlignBottom);
		const uint32_t cropAlign = t.boundsAlign & mask;

		if (cropFlipped) {
			if (cropAlign == kAlignCenter)
				cropOrigin *= 2;
			else if (cropAlign & (kAlignTop | kAlignLeft))
				cropOrigin -= cropDiff;
		} else {
			cropOrigin = 0;
		}

		if (cropWidth)
			ox = cropOrigin;
		else
			oy = cropOrigin;
	}

	ox += (width < 0.0f) ? width : 0.0f;
	oy += (height < 0.0f) ? height : 0.0f;
}

uint32_t croppedSize(uint32_t size, int a, int b)
{
	const uint32_t c = (uint32_t)std::max(0, a) + (uint32_t)std::max(0, b);
	return (c > size) ? 2 : (size - c);
}

Affine composeItem(float px, float py, double rot, float ox, float oy, float sx, float sy)
{
	return Affine::translation(px, py) * Affine::rotationDeg(rot) * Affine::translation(-ox, -oy) *
	       Affine::scaling(sx, sy);
}

ItemModel withTransform(const ItemModel &m, const ItemTransform &t)
{
	ItemModel r = m;
	r.t = t;
	return r;
}

ItemTransform keepAnchor(const ItemModel &before, const Affine &parent, ItemTransform after, const RefPoint &anchor)
{
	const Vec2 p0 = visualFrame(before, parent).point(anchor);
	const Vec2 p1 = visualFrame(withTransform(before, after), parent).point(anchor);
	after.pos += parent.inverted().mapVector(p0 - p1);
	return after;
}

ItemTransform keepCenter(const ItemModel &before, const Affine &parent, ItemTransform after)
{
	const Vec2 c0 = visualFrame(before, parent).center();
	const Vec2 c1 = visualFrame(withTransform(before, after), parent).center();
	after.pos += parent.inverted().mapVector(c0 - c1);
	return after;
}

bool hasArea(const ItemModel &m)
{
	return m.sourceWidth > 0 && m.sourceHeight > 0;
}

} // namespace

ItemGeometry computeGeometry(const ItemModel &m)
{
	const ItemTransform &t = m.t;
	ItemGeometry g;

	uint32_t cx = croppedSize(m.sourceWidth, t.crop.left, t.crop.right);
	uint32_t cy = croppedSize(m.sourceHeight, t.crop.top, t.crop.bottom);
	const uint32_t width = cx;
	const uint32_t height = cy;
	g.croppedWidth = cx;
	g.croppedHeight = cy;

	float ox = 0.0f, oy = 0.0f;
	float sx = (float)t.scale.x;
	float sy = (float)t.scale.y;
	const float px = (float)t.pos.x;
	const float py = (float)t.pos.y;

	if (t.boundsType != BoundsType::None && width > 0 && height > 0) {
		calculateBounds(t, ox, oy, sx, sy, cx, cy);
	} else {
		cx = obsFloatToU32((float)cx * sx);
		cy = obsFloatToU32((float)cy * sy);
	}

	addAlignment(ox, oy, t.align, (int)cx, (int)cy);
	g.draw = composeItem(px, py, t.rot, ox, oy, sx, sy);
	g.outputScale = {sx, sy};

	float bsx, bsy;
	if (t.boundsType != BoundsType::None) {
		bsx = (float)t.bounds.x;
		bsy = (float)t.bounds.y;
	} else {
		bsx = (float)t.scale.x * (float)width;
		bsy = (float)t.scale.y * (float)height;
	}
	g.boxScale = {bsx, bsy};

	float box = 0.0f, boy = 0.0f;
	addAlignment(box, boy, t.align, (int)bsx, (int)bsy);
	g.box = composeItem(px, py, t.rot, box, boy, bsx, bsy);
	return g;
}

Vec2 VisualFrame::point(const RefPoint &ref) const
{
	const Vec2 &tl = quad.p[0];
	return tl + (quad.p[1] - tl) * ref.fx + (quad.p[3] - tl) * ref.fy;
}

VisualFrame visualFrame(const ItemModel &m, const Affine &parent)
{
	const ItemGeometry g = computeGeometry(m);
	const Affine world = parent * g.box;
	const bool flipU = g.boxScale.x < 0;
	const bool flipV = g.boxScale.y < 0;
	auto P = [&](double u, double v) {
		return world.map({flipU ? 1.0 - u : u, flipV ? 1.0 - v : v});
	};

	VisualFrame f;
	f.quad = Quad{{P(0, 0), P(1, 0), P(1, 1), P(0, 1)}};
	const Vec2 ex = f.quad.p[1] - f.quad.p[0];
	const Vec2 ey = f.quad.p[3] - f.quad.p[0];
	f.width = ex.length();
	f.height = ey.length();
	f.rotation = (f.width > 0) ? normalizeDegrees(radToDeg(std::atan2(ex.y, ex.x))) : normalizeDegrees(m.t.rot);
	return f;
}

double worldRotation(const ItemModel &m, const Affine &parent)
{
	return normalizeDegrees(m.t.rot + parent.rotationDeg());
}

ItemTransform moveBy(const ItemModel &m, const Affine &parent, const Vec2 &worldDelta)
{
	ItemTransform t = m.t;
	t.pos += parent.inverted().mapVector(worldDelta);
	return t;
}

ItemTransform moveRefTo(const ItemModel &m, const Affine &parent, const RefPoint &ref, const Vec2 &worldTarget)
{
	const Vec2 current = visualFrame(m, parent).point(ref);
	return moveBy(m, parent, worldTarget - current);
}

ItemTransform setWorldSize(const ItemModel &m, const Affine &parent, double width, double height,
			   const RefPoint &anchor)
{
	if (!hasArea(m) || !(width > 0) || !(height > 0) || !std::isfinite(width) || !std::isfinite(height))
		return m.t;

	const ItemGeometry g = computeGeometry(m);
	const VisualFrame f = visualFrame(m, parent);
	if (!(f.width > 0) || !(f.height > 0))
		return m.t;

	const double localW = std::fabs(g.boxScale.x) * width / f.width;
	const double localH = std::fabs(g.boxScale.y) * height / f.height;

	ItemTransform t = m.t;
	if (t.boundsType != BoundsType::None) {
		t.bounds = {localW, localH};
	} else {
		if (g.croppedWidth == 0 || g.croppedHeight == 0)
			return m.t;
		t.scale.x = std::copysign(localW / (double)g.croppedWidth, m.t.scale.x);
		t.scale.y = std::copysign(localH / (double)g.croppedHeight, m.t.scale.y);
	}
	return keepAnchor(m, parent, t, anchor);
}

ItemTransform setFrame(const ItemModel &m, const Affine &parent, double width, double height, FrameMode mode,
		       const RefPoint &anchor)
{
	if (!hasArea(m) || !(width > 0) || !(height > 0) || !std::isfinite(width) || !std::isfinite(height))
		return m.t;
	const VisualFrame f = visualFrame(m, parent);
	const bool sameShape = f.width > 0 && f.height > 0 && std::fabs(width / height - f.width / f.height) < 1e-6;
	if (mode == FrameMode::Stretch || (sameShape && mode == FrameMode::Fill && m.t.boundsType == BoundsType::None))
		return setWorldSize(m, parent, width, height, anchor);

	/* World -> local (a group may scale its items). */
	const double px = std::hypot(parent.a, parent.b), py = std::hypot(parent.c, parent.d);
	const double w = width / (px > 1e-12 ? px : 1.0), h = height / (py > 1e-12 ? py : 1.0);
	const double sw = m.sourceWidth, sh = m.sourceHeight;
	/* The anchor is in visual space: a flipped axis keeps the opposite side of the source. */
	const double ax = m.t.scale.x < 0 ? 1.0 - anchor.fx : anchor.fx;
	const double ay = m.t.scale.y < 0 ? 1.0 - anchor.fy : anchor.fy;

	ItemTransform t = m.t;
	t.cropToBounds = false;
	if (mode == FrameMode::Fill) {
		const double s = std::max(w / sw, h / sh);
		const int ex = std::max(0, (int)std::lround(sw - w / s));
		const int ey = std::max(0, (int)std::lround(sh - h / s));
		t.crop.left = (int)std::lround(ex * ax);
		t.crop.right = ex - t.crop.left;
		t.crop.top = (int)std::lround(ey * ay);
		t.crop.bottom = ey - t.crop.top;
		t.boundsType = BoundsType::None;
		t.bounds = {};
		/* Whole source pixels are cropped: a tiny per-axis correction keeps the exact frame. */
		t.scale.x = std::copysign(w / (sw - ex), m.t.scale.x);
		t.scale.y = std::copysign(h / (sh - ey), m.t.scale.y);
	} else {
		t.crop = Crop();
		t.boundsType = BoundsType::ScaleInner;
		t.bounds = {w, h};
		t.boundsAlign = (anchor.fx < 0.25   ? kAlignLeft
				 : anchor.fx > 0.75 ? kAlignRight
						    : 0u) |
				(anchor.fy < 0.25   ? kAlignTop
				 : anchor.fy > 0.75 ? kAlignBottom
						    : 0u);
		t.scale = {std::copysign(1.0, m.t.scale.x), std::copysign(1.0, m.t.scale.y)};
	}
	return keepAnchor(m, parent, t, anchor);
}

ItemTransform setFrameRatio(const ItemModel &m, const Affine &parent, double ratio, FrameMode mode,
			    const RefPoint &anchor)
{
	const VisualFrame f = visualFrame(m, parent);
	if (!(ratio > 0) || !(f.width > 0) || !(f.height > 0))
		return m.t;
	double w = f.width, h = f.height;
	if (w / h > ratio)
		w = h * ratio;
	else
		h = w / ratio;
	return setFrame(m, parent, w, h, mode, anchor);
}

ItemTransform setWorldRotation(const ItemModel &m, const Affine &parent, double deg)
{
	if (!std::isfinite(deg))
		return m.t;
	ItemTransform t = m.t;
	t.rot = normalizeDegrees(deg - parent.rotationDeg());
	return keepCenter(m, parent, t);
}

ItemTransform rotateBy(const ItemModel &m, const Affine &parent, double deltaDeg)
{
	return setWorldRotation(m, parent, worldRotation(m, parent) + deltaDeg);
}

ItemTransform flip(const ItemModel &m, const Affine &parent, Axis axis)
{
	ItemTransform t = m.t;
	if (axis == Axis::X)
		t.scale.x = -t.scale.x;
	else
		t.scale.y = -t.scale.y;
	return keepCenter(m, parent, t);
}

ItemTransform setCrop(const ItemModel &m, const Affine &parent, const Crop &requested, const RefPoint &anchor)
{
	Crop c = requested;
	c.left = std::max(0, c.left);
	c.right = std::max(0, c.right);
	c.top = std::max(0, c.top);
	c.bottom = std::max(0, c.bottom);
	/* Never crop the whole source away: OBS would silently fall back to a 2 px texture. */
	const int w = (int)m.sourceWidth, h = (int)m.sourceHeight;
	if (w > 0 && c.left + c.right >= w) {
		c.left = std::min(c.left, w - 1);
		c.right = std::max(0, w - 1 - c.left);
	}
	if (h > 0 && c.top + c.bottom >= h) {
		c.top = std::min(c.top, h - 1);
		c.bottom = std::max(0, h - 1 - c.top);
	}

	ItemTransform t = m.t;
	t.crop = c;
	if (t.crop == m.t.crop)
		return m.t;

	if (m.t.boundsType != BoundsType::None)
		return keepAnchor(m, parent, t, anchor);

	/* Keep the pixels of the source where they are on the canvas. */
	const ItemGeometry g0 = computeGeometry(m);
	const ItemGeometry g1 = computeGeometry(withTransform(m, t));
	const Vec2 p0 = (parent * g0.draw).map({-(double)m.t.crop.left, -(double)m.t.crop.top});
	const Vec2 p1 = (parent * g1.draw).map({-(double)c.left, -(double)c.top});
	t.pos += parent.inverted().mapVector(p0 - p1);
	return t;
}

ItemTransform fitInto(const ItemModel &m, const Affine &parent, const Rect &area, FitMode mode)
{
	const VisualFrame f = visualFrame(m, parent);
	if (!(f.width > 0) || !(f.height > 0) || !(area.w > 0) || !(area.h > 0))
		return m.t;

	double w = area.w, h = area.h;
	if (mode != FitMode::Stretch) {
		const double kx = area.w / f.width;
		const double ky = area.h / f.height;
		const double k = (mode == FitMode::Fit) ? std::min(kx, ky) : std::max(kx, ky);
		w = f.width * k;
		h = f.height * k;
	}
	const RefPoint center{0.5, 0.5};
	ItemModel sized = withTransform(m, setWorldSize(m, parent, w, h, center));
	return moveRefTo(sized, parent, center, area.center());
}

ItemTransform originalSize(const ItemModel &m, const Affine &parent, const RefPoint &anchor)
{
	ItemTransform t = m.t;
	t.boundsType = BoundsType::None;
	t.scale = {std::copysign(1.0, m.t.scale.x), std::copysign(1.0, m.t.scale.y)};
	return keepAnchor(m, parent, t, anchor);
}

ItemTransform setBoundsType(const ItemModel &m, const Affine &parent, BoundsType type, const RefPoint &anchor)
{
	if (type == m.t.boundsType)
		return m.t;

	const ItemGeometry g = computeGeometry(m);
	ItemTransform t = m.t;
	if (m.t.boundsType == BoundsType::None) {
		t.bounds = {std::fabs(g.boxScale.x), std::fabs(g.boxScale.y)};
	} else if (type == BoundsType::None) {
		/* Keep the content at the size the bounds were giving it. */
		t.scale = g.outputScale;
	}
	t.boundsType = type;
	return keepAnchor(m, parent, t, anchor);
}

HandleResize resizeWithHandle(const Rect &start, const RefPoint &handle, const Vec2 &pointer, bool keepRatio,
			      bool fromCenter, double minSize)
{
	const bool mx = handle.fx != 0.5, my = handle.fy != 0.5;
	const RefPoint anchor{fromCenter || !mx ? 0.5 : 1.0 - handle.fx, fromCenter || !my ? 0.5 : 1.0 - handle.fy};
	const Vec2 a = pointInRect(start, anchor);
	if (!(start.w > 0) || !(start.h > 0) || !std::isfinite(pointer.x) || !std::isfinite(pointer.y))
		return {start, anchor};
	minSize = std::max(minSize, 1e-6);

	/* Signed sizes: positive while the pointer stays on the handle's side of the anchor. */
	const double span = fromCenter ? 2.0 : 1.0;
	double w = start.w, h = start.h;
	if (mx)
		w = (pointer.x - a.x) * (handle.fx > 0.5 ? 1.0 : -1.0) * span;
	if (my)
		h = (pointer.y - a.y) * (handle.fy > 0.5 ? 1.0 : -1.0) * span;

	if (keepRatio && (mx || my)) {
		double k;
		if (mx && my) /* projection on the diagonal: continuous whatever the direction */
			k = (w * start.w + h * start.h) / (start.w * start.w + start.h * start.h);
		else
			k = mx ? w / start.w : h / start.h;
		k = std::max(k, minSize / std::min(start.w, start.h));
		w = start.w * k;
		h = start.h * k;
	} else {
		w = std::max(w, minSize);
		h = std::max(h, minSize);
	}
	return {{a.x - w * anchor.fx, a.y - h * anchor.fy, w, h}, anchor};
}

std::string ratioLabel(double width, double height)
{
	if (!(width > 0) || !(height > 0) || !std::isfinite(width) || !std::isfinite(height))
		return {};
	const double r = width / height;
	/* Common ratios first, then any small integer ratio. */
	static const int kCommon[][2] = {{16, 9}, {9, 16}, {4, 3}, {3, 4}, {1, 1},
					 {21, 9}, {3, 2},  {2, 3}, {5, 4}, {4, 5}};
	const double tol = 0.005;
	for (const auto &c : kCommon) {
		if (std::fabs(r / ((double)c[0] / c[1]) - 1.0) < tol)
			return std::to_string(c[0]) + ":" + std::to_string(c[1]);
	}
	for (int d = 1; d <= 10; d++) {
		const double n = std::round(r * d);
		if (n >= 1 && n <= 32 && std::fabs(r / (n / d) - 1.0) < tol / 2)
			return std::to_string((int)n) + ":" + std::to_string(d);
	}
	char buf[32];
	if (r >= 1)
		std::snprintf(buf, sizeof buf, "%.2f:1", r);
	else
		std::snprintf(buf, sizeof buf, "1:%.2f", 1.0 / r);
	return buf;
}

} // namespace layouts::core
