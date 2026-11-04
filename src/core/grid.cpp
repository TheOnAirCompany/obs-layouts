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

#include "grid.hpp"

#include <cmath>

namespace layouts::core {

std::vector<Track> computeTracks(const TrackGrid &g, double extent)
{
	std::vector<Track> tracks;
	if (g.count < 1 || g.count > 1000 || !(extent > 0))
		return tracks;

	const int n = g.count;
	const double gutter = std::max(0.0, g.gutter);
	const double margin = std::max(0.0, g.margin);
	double size = 0.0;
	double start = 0.0;

	if (g.align == TrackAlign::Stretch) {
		size = (extent - 2.0 * margin - gutter * (n - 1)) / n;
		start = margin;
	} else {
		size = std::max(0.0, g.trackSize);
		const double total = size * n + gutter * (n - 1);
		switch (g.align) {
		case TrackAlign::Start:
			start = margin;
			break;
		case TrackAlign::End:
			start = extent - margin - total;
			break;
		case TrackAlign::Center:
		default:
			start = (extent - total) / 2.0;
			break;
		}
	}
	if (!(size > 0))
		return tracks;

	for (int i = 0; i < n; i++) {
		const double s = start + i * (size + gutter);
		tracks.push_back({s, s + size});
	}
	return tracks;
}

std::vector<double> squareLines(const SquareGrid &g, Axis axis, double from, double to)
{
	std::vector<double> lines;
	if (!(g.size > 0) || !(to >= from))
		return lines;
	const double off = axis == Axis::X ? g.offsetX : g.offsetY;
	const double first = std::ceil((from - off) / g.size);
	const double last = std::floor((to - off) / g.size);
	if (last - first > 20000)
		return lines; /* too dense to be useful */
	for (double k = first; k <= last; k += 1.0)
		lines.push_back(off + k * g.size);
	return lines;
}

double nearestSquareLine(const SquareGrid &g, Axis axis, double v)
{
	if (!(g.size > 0))
		return v;
	const double off = axis == Axis::X ? g.offsetX : g.offsetY;
	return off + std::round((v - off) / g.size) * g.size;
}

std::vector<double> trackEdges(const std::vector<Track> &tracks)
{
	std::vector<double> edges;
	edges.reserve(tracks.size() * 2);
	for (const Track &t : tracks) {
		edges.push_back(t.start);
		edges.push_back(t.end);
	}
	return edges;
}

RulerTicks chooseRulerTicks(double zoom, double minMajorPx, double minMinorPx)
{
	RulerTicks t;
	if (!(zoom > 0) || !std::isfinite(zoom))
		return t;

	static const double kSteps[] = {1, 2, 5};
	double major = 1.0;
	for (double decade = 1.0; decade <= 1e7; decade *= 10.0) {
		bool found = false;
		for (double s : kSteps) {
			if (s * decade * zoom >= minMajorPx) {
				major = s * decade;
				found = true;
				break;
			}
		}
		if (found)
			break;
	}
	t.major = major;

	/* Prefer 10, then 5, then 2 subdivisions, never below one canvas pixel. */
	t.subdivisions = 1;
	for (int sub : {10, 5, 2}) {
		const double minor = major / sub;
		if (minor >= 1.0 && minor * zoom >= minMinorPx && std::fmod(major, (double)sub) == 0.0) {
			t.subdivisions = sub;
			break;
		}
	}
	return t;
}

} // namespace layouts::core
