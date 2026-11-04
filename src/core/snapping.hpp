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

/* Smart snapping and measurements.
 *
 * Targets are grouped in priority tiers. Within the snap threshold, a target of a
 * higher tier always wins over a lower tier, then the closest target wins, then the
 * first anchor (start, center, end) wins. This makes results deterministic. */

#include "geometry.hpp"
#include "grid.hpp"

#include <optional>
#include <vector>

namespace layouts::core {

enum class SnapKind : int {
	Guide = 0,  /* user guides */
	Grid = 1,   /* square grid lines, column/row edges */
	Item = 2,   /* edges and centers of other items, equal spacing */
	Canvas = 3, /* canvas edges and center */
};

struct SnapSettings {
	bool enabled = true;
	bool guides = true;
	bool grid = false;
	bool items = true;
	bool canvas = true;
	bool spacing = true;
	double thresholdPx = 6.0; /* screen pixels */
};

struct SnapContext {
	Rect canvas;
	std::vector<double> guideX; /* vertical guides */
	std::vector<double> guideY; /* horizontal guides */
	std::optional<SquareGrid> square;
	std::vector<double> trackX; /* column edges */
	std::vector<double> trackY; /* row edges */
	std::vector<Rect> others;   /* items that are not being moved */
};

/* A line that the moving rectangle is aligned with after snapping. axis == X means a
 * vertical line at x = pos; from/to is its extent on the other axis (for drawing). */
struct SnapLine {
	Axis axis;
	double pos;
	SnapKind kind;
	double from;
	double to;
};

/* Equal gaps found by spacing snapping. Each gap runs along `axis` from start to end
 * at the `cross` coordinate. */
struct SpacingHint {
	Axis axis;
	double gap;
	struct Segment {
		double start;
		double end;
		double cross;
	};
	std::vector<Segment> segments;
};

struct SnapResult {
	Vec2 delta;
	bool snappedX = false;
	bool snappedY = false;
	std::vector<SnapLine> lines;
	std::vector<SpacingHint> spacings;
};

/* threshold is in canvas units (screen threshold / zoom). */
SnapResult snapMove(const Rect &moving, const SnapContext &ctx, const SnapSettings &s, double threshold,
		    bool allowX = true, bool allowY = true);

struct ValueSnap {
	double value;
	bool snapped = false;
	std::optional<SnapLine> line;
};

/* Snaps a single coordinate (e.g. an edge during resize, a guide being dragged).
 * crossFrom/crossTo give the extent of the edge on the other axis, for feedback. */
ValueSnap snapValue(double v, Axis axis, const SnapContext &ctx, const SnapSettings &s, double threshold,
		    double crossFrom, double crossTo);

/* ---- Measurements ---- */

struct Measure {
	Axis axis;    /* the distance runs along this axis */
	double start; /* along axis */
	double end;
	double cross; /* coordinate on the other axis */
	double value() const { return end - start; }
};

/* Distances between a (selected) and b (hovered or the canvas), Figma style:
 * containment gives edge-to-edge distances, separation gives the gap. */
std::vector<Measure> measureBetween(const Rect &a, const Rect &b);

} // namespace layouts::core
