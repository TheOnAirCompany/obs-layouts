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

/* Pure 2D geometry used by every Layouts engine. No Qt, no libobs. */

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace layouts::core {

constexpr double kPi = 3.14159265358979323846;

inline double degToRad(double d)
{
	return d * kPi / 180.0;
}
inline double radToDeg(double r)
{
	return r * 180.0 / kPi;
}

/* Normalizes an angle to (-180, 180]. */
double normalizeDegrees(double deg);

enum class Axis { X, Y };

struct Vec2 {
	double x = 0.0;
	double y = 0.0;

	constexpr Vec2() = default;
	constexpr Vec2(double x_, double y_) : x(x_), y(y_) {}

	Vec2 operator+(const Vec2 &o) const { return {x + o.x, y + o.y}; }
	Vec2 operator-(const Vec2 &o) const { return {x - o.x, y - o.y}; }
	Vec2 operator*(double s) const { return {x * s, y * s}; }
	Vec2 operator/(double s) const { return {x / s, y / s}; }
	Vec2 &operator+=(const Vec2 &o)
	{
		x += o.x;
		y += o.y;
		return *this;
	}
	Vec2 &operator-=(const Vec2 &o)
	{
		x -= o.x;
		y -= o.y;
		return *this;
	}
	bool operator==(const Vec2 &o) const { return x == o.x && y == o.y; }
	bool operator!=(const Vec2 &o) const { return !(*this == o); }

	double length() const { return std::hypot(x, y); }
	double get(Axis a) const { return a == Axis::X ? x : y; }
	void set(Axis a, double v) { (a == Axis::X ? x : y) = v; }
};

struct Rect {
	double x = 0.0;
	double y = 0.0;
	double w = 0.0;
	double h = 0.0;

	constexpr Rect() = default;
	constexpr Rect(double x_, double y_, double w_, double h_) : x(x_), y(y_), w(w_), h(h_) {}

	static Rect fromEdges(double l, double t, double r, double b) { return {l, t, r - l, b - t}; }
	static Rect boundingBox(const std::vector<Vec2> &points);

	double left() const { return x; }
	double top() const { return y; }
	double right() const { return x + w; }
	double bottom() const { return y + h; }
	double centerX() const { return x + w / 2.0; }
	double centerY() const { return y + h / 2.0; }
	Vec2 center() const { return {centerX(), centerY()}; }

	double start(Axis a) const { return a == Axis::X ? x : y; }
	double end(Axis a) const { return a == Axis::X ? right() : bottom(); }
	double mid(Axis a) const { return a == Axis::X ? centerX() : centerY(); }
	double size(Axis a) const { return a == Axis::X ? w : h; }

	Rect translated(const Vec2 &d) const { return {x + d.x, y + d.y, w, h}; }
	Rect united(const Rect &o) const;
	bool intersects(const Rect &o) const;
	bool contains(const Vec2 &p) const;
	bool contains(const Rect &o) const;
	bool isValid() const { return std::isfinite(x) && std::isfinite(y) && std::isfinite(w) && std::isfinite(h); }
	bool operator==(const Rect &o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
};

/* 2D affine transform, column-vector convention:
 *   x' = a*x + c*y + tx
 *   y' = b*x + d*y + ty
 * In OBS canvas space Y points down, so a positive rotation is clockwise on screen,
 * exactly like scene item rotation in OBS. */
struct Affine {
	double a = 1.0, b = 0.0, c = 0.0, d = 1.0, tx = 0.0, ty = 0.0;

	static Affine identity() { return {}; }
	static Affine translation(double x, double y);
	static Affine scaling(double sx, double sy);
	static Affine rotationDeg(double deg);

	Vec2 map(const Vec2 &p) const { return {a * p.x + c * p.y + tx, b * p.x + d * p.y + ty}; }
	Vec2 mapVector(const Vec2 &v) const { return {a * v.x + c * v.y, b * v.x + d * v.y}; }

	/* Returns this ∘ other: other is applied first. */
	Affine operator*(const Affine &o) const;

	double determinant() const { return a * d - b * c; }
	bool invertible() const { return std::fabs(determinant()) > 1e-12; }
	Affine inverted() const; /* identity if not invertible */

	/* Angle in degrees of the transformed X axis. */
	double rotationDeg() const { return radToDeg(std::atan2(b, a)); }
};

/* Four points in visual order: top-left, top-right, bottom-right, bottom-left. */
struct Quad {
	std::array<Vec2, 4> p;

	Rect bounds() const;
	Vec2 center() const { return (p[0] + p[2]) / 2.0; }
	bool contains(const Vec2 &pt) const;
	bool intersects(const Rect &r) const;
};

/* A point inside a box expressed as fractions: (0,0) top-left .. (1,1) bottom-right. */
struct RefPoint {
	double fx = 0.0;
	double fy = 0.0;

	static RefPoint fromIndex(int index); /* 0..8, row-major, 4 = center */
	int toIndex() const;
	bool operator==(const RefPoint &o) const { return fx == o.fx && fy == o.fy; }
};

Vec2 pointInRect(const Rect &r, const RefPoint &ref);

bool nearlyEqual(double a, double b, double eps = 1e-6);

} // namespace layouts::core
