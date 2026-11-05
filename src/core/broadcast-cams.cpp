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

#include "broadcast-cams.hpp"

#include <algorithm>
#include <cmath>

namespace layouts::core {

namespace {

constexpr double kRefW = 1920.0, kRefH = 1080.0; /* the reference the templates are drawn on */
constexpr double k169 = 16.0 / 9.0;

Rect insetBy(const Rect &r, double d)
{
	d = std::max(0.0, std::min(d, std::min(r.w, r.h) / 4));
	return {r.x + d, r.y + d, r.w - 2 * d, r.h - 2 * d};
}

/* Largest rect of aspect a inside r, centered. */
Rect fit(double a, const Rect &r)
{
	double w = r.w, h = r.w / a;
	if (h > r.h) {
		h = r.h;
		w = h * a;
	}
	return {r.x + (r.w - w) / 2, r.y + (r.h - h) / 2, w, h};
}

std::vector<Rect> split(const Rect &r, int n, double g, Axis axis)
{
	std::vector<Rect> out;
	if (axis == Axis::X) {
		const double w = (r.w - g * (n - 1)) / n;
		for (int i = 0; i < n; i++)
			out.push_back({r.x + i * (w + g), r.y, w, r.h});
	} else {
		const double h = (r.h - g * (n - 1)) / n;
		for (int i = 0; i < n; i++)
			out.push_back({r.x, r.y + i * (h + g), r.w, h});
	}
	return out;
}

std::vector<Rect> grid(const Rect &r, int cols, int rows, double g)
{
	std::vector<Rect> out;
	for (const Rect &row : split(r, rows, g, Axis::Y))
		for (const Rect &c : split(row, cols, g, Axis::X))
			out.push_back(c);
	return out;
}

/* Rows of tiles of aspect a (tiles per row in `counts`), as large as fits, centered as a block. */
std::vector<Rect> rowsOf(const std::vector<int> &counts, double a, const Rect &r, double g)
{
	const int rows = (int)counts.size();
	const int maxN = *std::max_element(counts.begin(), counts.end());
	double w = (r.w - g * (maxN - 1)) / maxN, h = w / a;
	if (rows * h + (rows - 1) * g > r.h) {
		h = (r.h - g * (rows - 1)) / rows;
		w = h * a;
	}
	const double th = rows * h + (rows - 1) * g;
	std::vector<Rect> out;
	for (int i = 0; i < rows; i++) {
		const int n = counts[i];
		const double tw = n * w + (n - 1) * g;
		for (int k = 0; k < n; k++)
			out.push_back({r.x + (r.w - tw) / 2 + k * (w + g), r.y + (r.h - th) / 2 + i * (h + g), w, h});
	}
	return out;
}

/* n tiles of aspect a in one centered row. */
std::vector<Rect> rowOf(int n, double a, const Rect &r, double g)
{
	return rowsOf({n}, a, r, g);
}

/* n tiles of aspect a in one centered column. */
std::vector<Rect> colOf(int n, double a, const Rect &r, double g)
{
	double h = (r.h - g * (n - 1)) / n, w = h * a;
	if (w > r.w) {
		w = r.w;
		h = w / a;
	}
	const double th = n * h + (n - 1) * g;
	std::vector<Rect> out;
	for (int i = 0; i < n; i++)
		out.push_back({r.x + (r.w - w) / 2, r.y + (r.h - th) / 2 + i * (h + g), w, h});
	return out;
}

/* The content area and the scale from the 1920 x 1080 reference. */
struct Ref {
	Rect r;    /* canvas inset by spec.margin */
	double u;  /* scale of fixed insets and gaps */
	double g;  /* spec.gap */
	double sx; /* reference x to content x */
	double sy;

	Rect at(double x, double y, double w, double h) const { return {r.x + x * sx, r.y + y * sy, w * sx, h * sy}; }
	Rect in(double d) const { return insetBy(r, d * u); }
	/* A 16:9 inset of reference width w in the bottom right corner (40 px from the edges). */
	Rect pip(double w) const
	{
		const double pw = w * u, ph = pw / k169, m = 40 * u;
		return {r.right() - m - pw, r.bottom() - m - ph, pw, ph};
	}
};

void add(std::vector<Slot> &out, const Rect &cell, bool host = false)
{
	Slot s{SlotKind::Camera, cell};
	s.host = host;
	out.push_back(s);
}

/* The first cell is the host, the others the guests. */
void hostFirst(std::vector<Slot> &out, const std::vector<Rect> &cells)
{
	for (size_t i = 0; i < cells.size(); i++)
		add(out, cells[i], i == 0);
}

void hostThen(std::vector<Slot> &out, const Rect &host, const std::vector<Rect> &guests)
{
	add(out, host, true);
	for (const Rect &c : guests)
		add(out, c);
}

/* Host in a wide 16:9 on the left (3/4 of the width), n guests stacked on the right. */
void hostColumn(std::vector<Slot> &out, const Ref &f, int n)
{
	const Rect &r = f.r;
	const double sw = (r.w - f.g) * 0.75;
	const Rect h = fit(k169, {r.x, r.y, sw, r.h});
	hostThen(out, h, split({r.x + sw + f.g, h.y, r.w - sw - f.g, h.h}, n, f.g, Axis::Y));
}

struct CamTemplate {
	const char *key;
	int cameras;
};

/* Display order: the templates validated on the 0.5 review first, then the new ones by
 * number of cameras. */
const CamTemplate kTemplates[] = {
	{"Cam2Portrait", 2},   {"Cam2Square", 2},     {"Cam3Square", 3},      {"Cam2HostPip", 2},
	{"Cam1Strip2", 3},     {"Cam1Strip3", 4},     {"Cam1Strip4", 5},      {"Cam1Strip5", 6},
	{"Cam1Framed", 1},     {"Cam1Square", 1},     {"Cam1Portrait", 1},    {"Cam1TitleRight", 1},
	{"Cam1TitleBottom", 1}, {"Cam1Cinema", 1},    {"Cam2Stacked", 2},     {"Cam2Wide", 2},
	{"Cam2GuestPip", 2},   {"Cam2Cinema", 2},     {"Cam3Portrait", 3},    {"Cam3HostPip2", 3},
	{"Cam3HostColumn", 3}, {"Cam3TwoOne", 3},     {"Cam3Rows", 3},        {"Cam4Portrait", 4},
	{"Cam4SquareRow", 4},  {"Cam4SquareGrid", 4}, {"Cam4HostColumn", 4},  {"Cam5Portrait", 5},
	{"Cam5Square", 5},     {"Cam5HostColumn", 5}, {"Cam6Square", 6},      {"Cam6HostL", 6},
	{"Cam6TwoFour", 6},
};

} // namespace

void appendCamTemplates(std::vector<BroadcastTemplate> &list)
{
	for (const CamTemplate &t : kTemplates) {
		BroadcastTemplate b{t.key, t.cameras, 0};
		b.landscapeOnly = true;
		list.push_back(b);
	}
}

bool buildCamTemplate(const std::string &key, const Rect &canvas, const BroadcastSpec &spec, std::vector<Slot> &out)
{
	const bool known = std::any_of(std::begin(kTemplates), std::end(kTemplates),
				       [&](const CamTemplate &t) { return key == t.key; });
	if (!known || !(canvas.w > 0) || !(canvas.h > 0))
		return false;

	Ref f;
	f.r = insetBy(canvas, spec.margin);
	f.u = std::min(canvas.w / kRefW, canvas.h / kRefH);
	f.g = std::max(0.0, spec.gap);
	f.sx = f.r.w / kRefW;
	f.sy = f.r.h / kRefH;
	const Rect &r = f.r;
	const double u = f.u, g = f.g;

	/* Validated on the 0.5 review. */
	if (key == "Cam2Portrait") {
		hostFirst(out, rowOf(2, 4.0 / 5.0, f.in(100), 40 * u));
	} else if (key == "Cam2Square") {
		hostFirst(out, rowOf(2, 1, f.in(120), 40 * u));
	} else if (key == "Cam3Square") {
		hostFirst(out, rowOf(3, 1, f.in(60), 30 * u));
	} else if (key == "Cam2HostPip") {
		/* Back to front: the guest fills the canvas, the host is the inset on top. */
		add(out, r);
		add(out, f.pip(480), true);
	} else if (key.rfind("Cam1Strip", 0) == 0) {
		const int n = key[9] - '0';
		hostThen(out, fit(k169, f.at(0, 40, 1920, 720)), rowOf(n, k169, f.at(80, 790, 1760, 250), 24 * u));
	}
	/* One camera. */
	else if (key == "Cam1Framed") {
		hostFirst(out, {fit(k169, f.in(80))});
	} else if (key == "Cam1Square") {
		hostFirst(out, {fit(1, f.in(100))});
	} else if (key == "Cam1Portrait") {
		hostFirst(out, {fit(9.0 / 16.0, f.in(40))});
	} else if (key == "Cam1TitleRight") {
		hostFirst(out, {fit(k169, f.at(60, 60, 1180, 960))});
	} else if (key == "Cam1TitleBottom") {
		hostFirst(out, {fit(k169, f.at(0, 40, 1920, 760))});
	} else if (key == "Cam1Cinema") {
		hostFirst(out, {fit(2.39, r)});
	}
	/* Two cameras. */
	else if (key == "Cam2Stacked") {
		hostFirst(out, colOf(2, k169, f.in(40), 24 * u));
	} else if (key == "Cam2Wide") {
		const double bw = (r.w - g) * 2.0 / 3.0;
		hostFirst(out, {{r.x, r.y, bw, r.h}, {r.x + bw + g, r.y, r.w - bw - g, r.h}});
	} else if (key == "Cam2GuestPip") {
		/* Back to front: the host fills the canvas, the guest is the inset on top. */
		hostThen(out, r, {f.pip(480)});
	} else if (key == "Cam2Cinema") {
		hostFirst(out, split(r, 2, g, Axis::Y));
	}
	/* Three cameras. */
	else if (key == "Cam3Portrait") {
		hostFirst(out, rowOf(3, 4.0 / 5.0, f.in(80), 32 * u));
	} else if (key == "Cam3HostPip2") {
		hostThen(out, r, {f.at(1440, 490, 440, 248), f.at(1440, 762, 440, 248)});
	} else if (key == "Cam3HostColumn") {
		hostColumn(out, f, 2);
	} else if (key == "Cam3TwoOne") {
		hostFirst(out, rowsOf({2, 1}, k169, f.in(30), 20 * u));
	} else if (key == "Cam3Rows") {
		hostFirst(out, split(r, 3, g, Axis::Y));
	}
	/* Four cameras. */
	else if (key == "Cam4Portrait") {
		hostFirst(out, rowOf(4, 9.0 / 16.0, f.in(60), 24 * u));
	} else if (key == "Cam4SquareRow") {
		hostFirst(out, rowOf(4, 1, f.in(40), 24 * u));
	} else if (key == "Cam4SquareGrid") {
		hostFirst(out, rowsOf({2, 2}, 1, f.in(40), 24 * u));
	} else if (key == "Cam4HostColumn") {
		hostColumn(out, f, 3);
	}
	/* Five cameras. */
	else if (key == "Cam5Portrait") {
		hostFirst(out, rowOf(5, 9.0 / 16.0, f.in(40), 20 * u));
	} else if (key == "Cam5Square") {
		hostFirst(out, rowsOf({3, 2}, 1, f.in(30), 20 * u));
	} else if (key == "Cam5HostColumn") {
		hostColumn(out, f, 4);
	}
	/* Six cameras. */
	else if (key == "Cam6Square") {
		hostFirst(out, rowsOf({3, 3}, 1, f.in(30), 20 * u));
	} else if (key == "Cam6HostL") {
		/* The host spans 2 x 2 cells of a 3 x 3 grid, the five guests make an L around. */
		const std::vector<Rect> c = grid(r, 3, 3, g);
		const Rect big = Rect::fromEdges(c[0].x, c[0].y, c[4].right(), c[4].bottom());
		hostThen(out, big, {c[2], c[5], c[6], c[7], c[8]});
	} else if (key == "Cam6TwoFour") {
		const Rect top{r.x, r.y, r.w, (r.h - g) * 0.6};
		std::vector<Rect> cells = split(top, 2, g, Axis::X);
		for (const Rect &c : split({r.x, top.bottom() + g, r.w, r.h - top.h - g}, 4, g, Axis::X))
			cells.push_back(c);
		hostFirst(out, cells);
	}
	return true;
}

} // namespace layouts::core
