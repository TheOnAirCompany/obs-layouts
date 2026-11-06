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

#include "shape.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace layouts::core::shape {

double radiusFor(RadiusMode mode, double px, double w, double h)
{
	const double shortSide = std::max(0.0, std::min(w, h));
	switch (mode) {
	case RadiusMode::None:
		return 0.0;
	case RadiusMode::Round:
		return shortSide * 0.5;
	case RadiusMode::Pixels:
		return std::clamp(px, 0.0, shortSide * 0.5);
	case RadiusMode::Auto:
	default:
		return std::min(std::clamp(shortSide * kAutoFraction, kAutoMin, kAutoMax), shortSide * 0.5);
	}
}

Vec2 axisScale(double xx, double xy, double yx, double yy)
{
	return {std::hypot(xx, xy), std::hypot(yx, yy)};
}

bool axisAligned(double xx, double xy, double yx, double yy)
{
	const Vec2 s = axisScale(xx, xy, yx, yy);
	const double eps = 1e-4 * std::max(1.0, std::max(s.x, s.y));
	return std::fabs(xy) <= eps && std::fabs(yx) <= eps;
}

namespace {

/* > 0: o -> a -> b turns clockwise on screen (y down). */
double cross(Vec2 o, Vec2 a, Vec2 b)
{
	return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

} // namespace

std::vector<Vec2> convexPolygon(const std::vector<Vec2> &points)
{
	std::vector<Vec2> pts;
	for (const Vec2 &p : points)
		if (std::isfinite(p.x) && std::isfinite(p.y))
			pts.push_back(p);
	std::sort(pts.begin(), pts.end(), [](Vec2 a, Vec2 b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
	pts.erase(
		std::unique(pts.begin(), pts.end(),
			    [](Vec2 a, Vec2 b) { return std::fabs(a.x - b.x) < 1e-9 && std::fabs(a.y - b.y) < 1e-9; }),
		pts.end());
	if (pts.size() < 3)
		return {};
	/* Monotone chain, keeping clockwise turns only. */
	std::vector<Vec2> hull(2 * pts.size());
	size_t k = 0;
	for (size_t i = 0; i < pts.size(); i++) {
		while (k >= 2 && cross(hull[k - 2], hull[k - 1], pts[i]) <= 1e-12)
			k--;
		hull[k++] = pts[i];
	}
	for (size_t i = pts.size() - 1, t = k + 1; i > 0; i--) {
		while (k >= t && cross(hull[k - 2], hull[k - 1], pts[i - 1]) <= 1e-12)
			k--;
		hull[k++] = pts[i - 1];
	}
	hull.resize(k - 1);
	if (hull.size() < 3)
		return {};
	/* Too many corners: drop the ones that change the shape least. */
	while ((int)hull.size() > kMaxPolygonPoints) {
		size_t best = 0;
		double bestArea = std::numeric_limits<double>::infinity();
		for (size_t i = 0; i < hull.size(); i++) {
			const Vec2 a = hull[(i + hull.size() - 1) % hull.size()];
			const Vec2 c = hull[(i + 1) % hull.size()];
			const double area = std::fabs(cross(a, hull[i], c));
			if (area < bestArea) {
				bestArea = area;
				best = i;
			}
		}
		hull.erase(hull.begin() + (std::ptrdiff_t)best);
	}
	return hull;
}

double polygonDistance(const std::vector<Vec2> &poly, Vec2 p)
{
	double d = -std::numeric_limits<double>::infinity();
	if (poly.size() < 3)
		return d;
	for (size_t i = 0; i < poly.size(); i++) {
		const Vec2 a = poly[i], b = poly[(i + 1) % poly.size()];
		const double ex = b.x - a.x, ey = b.y - a.y;
		const double len = std::hypot(ex, ey);
		if (len < 1e-12)
			continue;
		/* Clockwise on screen: the inside is on the right of a -> b, (-ey, ex). */
		const double inside = ((p.x - a.x) * -ey + (p.y - a.y) * ex) / len;
		d = std::max(d, -inside);
	}
	return d;
}

std::vector<Vec2> denormalize(const std::vector<Vec2> &poly, double left, double top, double right, double bottom)
{
	std::vector<Vec2> out;
	out.reserve(poly.size());
	for (const Vec2 &p : poly)
		out.push_back({left + p.x * (right - left), top + p.y * (bottom - top)});
	return out;
}

} // namespace layouts::core::shape
