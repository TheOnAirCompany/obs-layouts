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

/* Layout grids (square, columns, rows) and ruler graduation. */

#include "geometry.hpp"

#include <vector>

namespace layouts::core {

enum class GridKind { Square, Columns, Rows };

enum class TrackAlign { Stretch, Start, Center, End };

struct SquareGrid {
	double size = 10.0;
	double offsetX = 0.0;
	double offsetY = 0.0;
};

/* Design-tool style column (or row) grid. With TrackAlign::Stretch the track size
 * is computed from count, gutter and margin; otherwise `trackSize` is used and the
 * group of tracks is aligned in the canvas (margin applies on the aligned side). */
struct TrackGrid {
	int count = 12;
	double gutter = 20.0;
	double margin = 40.0;
	double trackSize = 80.0;
	TrackAlign align = TrackAlign::Stretch;
};

struct Track {
	double start;
	double end;
};

std::vector<Track> computeTracks(const TrackGrid &g, double canvasExtent);

/* Lines of a square grid within [from, to] along one axis. */
std::vector<double> squareLines(const SquareGrid &g, Axis axis, double from, double to);

/* Nearest square grid line to v. */
double nearestSquareLine(const SquareGrid &g, Axis axis, double v);

/* Snap targets of a track grid: every track start and end. */
std::vector<double> trackEdges(const std::vector<Track> &tracks);

/* ---- Rulers ---- */

struct RulerTicks {
	double major = 100.0;  /* canvas units between labeled ticks */
	int subdivisions = 10; /* minor ticks per major interval (1 = none) */
};

/* zoom = screen pixels per canvas pixel. minMajorPx = minimum distance between two
 * labeled ticks on screen. Majors follow 1-2-5 steps. */
RulerTicks chooseRulerTicks(double zoom, double minMajorPx = 64.0, double minMinorPx = 6.0);

} // namespace layouts::core
