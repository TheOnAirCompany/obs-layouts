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

#include "geometry.hpp"

namespace layouts::core {

double normalizeDegrees(double deg)
{
	if (!std::isfinite(deg))
		return 0.0;
	double r = std::fmod(deg, 360.0);
	if (r <= -180.0)
		r += 360.0;
	else if (r > 180.0)
		r -= 360.0;
	return r;
}

Rect Rect::boundingBox(const std::vector<Vec2> &points)
{
	if (points.empty())
		return {};
	double l = points[0].x, r = points[0].x, t = points[0].y, b = points[0].y;
	for (const Vec2 &p : points) {
		l = std::min(l, p.x);
		r = std::max(r, p.x);
		t = std::min(t, p.y);
		b = std::max(b, p.y);
	}
	return fromEdges(l, t, r, b);
}

Rect Rect::united(const Rect &o) const
{
	return fromEdges(std::min(left(), o.left()), std::min(top(), o.top()), std::max(right(), o.right()),
			 std::max(bottom(), o.bottom()));
}

bool Rect::intersects(const Rect &o) const
{
	return left() <= o.right() && o.left() <= right() && top() <= o.bottom() && o.top() <= bottom();
}

bool Rect::contains(const Vec2 &p) const
{
	return p.x >= left() && p.x <= right() && p.y >= top() && p.y <= bottom();
}

bool Rect::contains(const Rect &o) const
{
	return o.left() >= left() && o.right() <= right() && o.top() >= top() && o.bottom() <= bottom();
}

Affine Affine::translation(double x, double y)
{
	Affine m;
	m.tx = x;
	m.ty = y;
	return m;
}

Affine Affine::scaling(double sx, double sy)
{
	Affine m;
	m.a = sx;
	m.d = sy;
	return m;
}

Affine Affine::rotationDeg(double deg)
{
	const double r = degToRad(deg);
	const double cs = std::cos(r);
	const double sn = std::sin(r);
	Affine m;
	m.a = cs;
	m.b = sn;
	m.c = -sn;
	m.d = cs;
	return m;
}

Affine Affine::operator*(const Affine &o) const
{
	Affine m;
	m.a = a * o.a + c * o.b;
	m.b = b * o.a + d * o.b;
	m.c = a * o.c + c * o.d;
	m.d = b * o.c + d * o.d;
	m.tx = a * o.tx + c * o.ty + tx;
	m.ty = b * o.tx + d * o.ty + ty;
	return m;
}

Affine Affine::inverted() const
{
	const double det = determinant();
	if (std::fabs(det) <= 1e-12)
		return {};
	Affine m;
	m.a = d / det;
	m.b = -b / det;
	m.c = -c / det;
	m.d = a / det;
	m.tx = -(m.a * tx + m.c * ty);
	m.ty = -(m.b * tx + m.d * ty);
	return m;
}

Rect Quad::bounds() const
{
	return Rect::boundingBox({p[0], p[1], p[2], p[3]});
}

static double cross(const Vec2 &o, const Vec2 &a, const Vec2 &b)
{
	return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

bool Quad::contains(const Vec2 &pt) const
{
	/* Works for convex quads in either winding. */
	bool hasNeg = false, hasPos = false;
	for (int i = 0; i < 4; i++) {
		const double c = cross(p[i], p[(i + 1) % 4], pt);
		hasNeg = hasNeg || c < 0;
		hasPos = hasPos || c > 0;
	}
	return !(hasNeg && hasPos);
}

static bool segmentsIntersect(const Vec2 &a, const Vec2 &b, const Vec2 &c, const Vec2 &d)
{
	const double d1 = cross(c, d, a);
	const double d2 = cross(c, d, b);
	const double d3 = cross(a, b, c);
	const double d4 = cross(a, b, d);
	return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0));
}

bool Quad::intersects(const Rect &r) const
{
	if (!bounds().intersects(r))
		return false;
	for (const Vec2 &pt : p) {
		if (r.contains(pt))
			return true;
	}
	const Quad rq{{Vec2{r.left(), r.top()}, Vec2{r.right(), r.top()}, Vec2{r.right(), r.bottom()},
		       Vec2{r.left(), r.bottom()}}};
	for (const Vec2 &pt : rq.p) {
		if (contains(pt))
			return true;
	}
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			if (segmentsIntersect(p[i], p[(i + 1) % 4], rq.p[j], rq.p[(j + 1) % 4]))
				return true;
		}
	}
	return false;
}

RefPoint RefPoint::fromIndex(int index)
{
	index = std::clamp(index, 0, 8);
	return {(index % 3) * 0.5, (index / 3) * 0.5};
}

int RefPoint::toIndex() const
{
	const int col = std::clamp((int)std::lround(fx * 2.0), 0, 2);
	const int row = std::clamp((int)std::lround(fy * 2.0), 0, 2);
	return row * 3 + col;
}

Vec2 pointInRect(const Rect &r, const RefPoint &ref)
{
	return {r.x + r.w * ref.fx, r.y + r.h * ref.fy};
}

bool nearlyEqual(double a, double b, double eps)
{
	return std::fabs(a - b) <= eps;
}

} // namespace layouts::core
