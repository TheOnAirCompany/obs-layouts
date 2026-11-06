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

/* Shape of a masked item (0.5): the corner radius for a visible size, the convex polygon
 * of diagonal layouts, and the scale of a render matrix. Pure math shared by the corner
 * filter (render thread) and the unit tests; the GPU effect mirrors polygonDistance.
 * core::styleRadius (style.cpp) is radiusFor(s.radiusMode, s.radiusPx, w, h). */

#include "geometry.hpp"
#include "style.hpp"

#include <vector>

namespace layouts::core::shape {

constexpr double kAutoFraction = 0.04; /* Auto: 4 % of the short side */
constexpr double kAutoMin = 6.0;       /* canvas pixels */
constexpr double kAutoMax = 48.0;
constexpr int kMaxPolygonPoints = 8;

/* Corner radius in canvas pixels for a visible size w x h (canvas pixels). Pixels uses px
 * (never more than half the short side); Round gives circles and pills. */
double radiusFor(RadiusMode mode, double px, double w, double h);

/* Scale of the two axes of a 2D linear map whose basis images are (xx, xy) and (yx, yy)
 * (rows x and y of an OBS matrix4): their lengths, so a rotated matrix keeps its scale. */
Vec2 axisScale(double xx, double xy, double yx, double yy);

/* True when the map has no rotation or shear (within tolerance). */
bool axisAligned(double xx, double xy, double yx, double yy);

/* A usable mask polygon: the convex hull of the points, clockwise on screen (y down),
 * at most kMaxPolygonPoints (fewer than 3 distinct points: empty, i.e. no polygon). */
std::vector<Vec2> convexPolygon(const std::vector<Vec2> &points);

/* Signed distance to a convex polygon from convexPolygon (negative inside): the largest
 * signed distance to its edge lines, exact inside and along the edges. */
double polygonDistance(const std::vector<Vec2> &poly, Vec2 p);

/* Points normalized to a rectangle (0..1 across it) -> absolute coordinates. */
std::vector<Vec2> denormalize(const std::vector<Vec2> &poly, double left, double top, double right, double bottom);

} // namespace layouts::core::shape
