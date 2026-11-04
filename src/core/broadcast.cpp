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

#include "broadcast.hpp"

#include <algorithm>
#include <cmath>

namespace layouts::core {

namespace {

constexpr double k169 = 16.0 / 9.0;

Rect inset(const Rect &r, double d)
{
	d = std::max(0.0, std::min(d, std::min(r.w, r.h) / 4));
	return {r.x + d, r.y + d, r.w - 2 * d, r.h - 2 * d};
}

/* n equal columns (or rows) inside r. */
std::vector<Rect> split(const Rect &r, int n, double g, Axis axis)
{
	std::vector<Rect> out;
	if (n < 1)
		return out;
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
		for (const Rect &cell : split(row, cols, g, Axis::X))
			out.push_back(cell);
	return out;
}

/* Largest 16:9 rect inside r, centered. */
Rect fit169(const Rect &r)
{
	double w = r.w, h = r.w / k169;
	if (h > r.h) {
		h = r.h;
		w = h * k169;
	}
	return {r.x + (r.w - w) / 2, r.y + (r.h - h) / 2, w, h};
}

void cams(std::vector<Slot> &out, const std::vector<Rect> &cells)
{
	for (const Rect &c : cells)
		out.push_back({SlotKind::Camera, c});
}

} // namespace

const std::vector<BroadcastTemplate> &broadcastTemplates()
{
	static const std::vector<BroadcastTemplate> list = [] {
		std::vector<BroadcastTemplate> l = {
			{"Cam1Full", 1, 0},       {"Cam2Split", 2, 0},       {"Cam2Duo169", 2, 0},
			{"Cam3Columns", 3, 0},    {"Cam3OneTwo", 3, 0},      {"Cam4Grid", 4, 0},
			{"Cam4OneThree", 4, 0},   {"Cam5TwoThree", 5, 0},    {"Cam5OneFour", 5, 0},
			{"Cam6Grid", 6, 0},       {"Screen1Full", 0, 1},     {"Screen2Split", 0, 2},
			{"Screen1Cam1Pip", 1, 1}, {"Screen1Cam1Side", 1, 1}, {"Screen2Cam2Grid", 2, 2},
		};
		for (int n = 2; n <= 4; n++)
			l.push_back({"Screen1Cam" + std::to_string(n) + "Column", n, 1});
		for (int n = 2; n <= 6; n++)
			l.push_back({"Screen1Cam" + std::to_string(n) + "Strip", n, 1});
		return l;
	}();
	return list;
}

std::vector<BroadcastTemplate> templatesFor(int cameras, int screens)
{
	std::vector<BroadcastTemplate> out;
	for (const BroadcastTemplate &t : broadcastTemplates()) {
		if (t.cameras == cameras && t.screens == screens)
			out.push_back(t);
	}
	return out;
}

static std::vector<Slot> buildLandscape(const std::string &key, const Rect &canvas, const BroadcastSpec &spec)
{
	std::vector<Slot> out;
	if (!(canvas.w > 0) || !(canvas.h > 0))
		return out;
	const Rect r = inset(canvas, spec.margin);
	const double g = std::max(0.0, spec.gap);

	if (key == "Cam1Full") {
		cams(out, {r});
	} else if (key == "Cam2Split") {
		cams(out, split(r, 2, g, Axis::X));
	} else if (key == "Cam2Duo169") {
		for (const Rect &half : split(r, 2, g, Axis::X))
			out.push_back({SlotKind::Camera, fit169(half)});
	} else if (key == "Cam3Columns") {
		cams(out, split(r, 3, g, Axis::X));
	} else if (key == "Cam3OneTwo") {
		const double bigW = (r.w - g) * 2.0 / 3.0;
		cams(out, {{r.x, r.y, bigW, r.h}});
		cams(out, split({r.x + bigW + g, r.y, r.w - bigW - g, r.h}, 2, g, Axis::Y));
	} else if (key == "Cam4Grid") {
		cams(out, grid(r, 2, 2, g));
	} else if (key == "Cam4OneThree") {
		const double bigH = (r.h - g) * 2.0 / 3.0;
		cams(out, {{r.x, r.y, r.w, bigH}});
		cams(out, split({r.x, r.y + bigH + g, r.w, r.h - bigH - g}, 3, g, Axis::X));
	} else if (key == "Cam5TwoThree") {
		const std::vector<Rect> rows = split(r, 2, g, Axis::Y);
		cams(out, split(rows[0], 2, g, Axis::X));
		cams(out, split(rows[1], 3, g, Axis::X));
	} else if (key == "Cam5OneFour") {
		const std::vector<Rect> halves = split(r, 2, g, Axis::X);
		cams(out, {halves[0]});
		cams(out, grid(halves[1], 2, 2, g));
	} else if (key == "Cam6Grid") {
		cams(out, grid(r, 3, 2, g));
	} else if (key == "Screen1Full") {
		out.push_back({SlotKind::Screen, r});
	} else if (key == "Screen2Split") {
		for (const Rect &half : split(r, 2, g, Axis::X))
			out.push_back({SlotKind::Screen, fit169(half)});
	} else if (key == "Screen1Cam1Pip") {
		const double w = canvas.w * 0.25;
		const double h = w / k169;
		const double pad = std::max(spec.margin, canvas.h * 0.035);
		out.push_back({SlotKind::Camera, {canvas.right() - pad - w, canvas.bottom() - pad - h, w, h}});
		out.push_back({SlotKind::Screen, r});
	} else if (key == "Screen1Cam1Side" ||
		   (key.rfind("Screen1Cam", 0) == 0 && key.find("Column") != std::string::npos)) {
		const int n = key == "Screen1Cam1Side" ? 1 : std::max(1, key[10] - '0');
		const double screenW = (r.w - g) * 0.75;
		const Rect screen = fit169({r.x, r.y, screenW, r.h});
		const Rect column{r.x + screenW + g, screen.y, r.w - screenW - g, screen.h};
		cams(out, split(column, n, g, Axis::Y));
		out.push_back({SlotKind::Screen, screen});
	} else if (key.rfind("Screen1Cam", 0) == 0 && key.find("Strip") != std::string::npos) {
		const int n = std::max(1, key[10] - '0');
		const double stripH = std::min((r.h - g) * 0.28, (r.w - g * (n - 1)) / n / k169);
		const Rect top{r.x, r.y, r.w, r.h - stripH - g};
		cams(out, split({r.x, r.bottom() - stripH, r.w, stripH}, n, g, Axis::X));
		out.push_back({SlotKind::Screen, fit169(top)});
	} else if (key == "Screen2Cam2Grid") {
		const std::vector<Rect> cells = grid(r, 2, 2, g);
		cams(out, {cells[2], cells[3]});
		out.push_back({SlotKind::Screen, fit169(cells[0])});
		out.push_back({SlotKind::Screen, fit169(cells[1])});
	}
	return out;
}

namespace {

Rect transposed(const Rect &r)
{
	return {r.y, r.x, r.h, r.w};
}

/* n tiles in a block: `rows` rows, the last row shares its width between what is left. */
std::vector<Rect> rowsOf(const Rect &r, int n, int rows, double g)
{
	std::vector<Rect> out;
	rows = std::clamp(rows, 1, std::max(1, n));
	const int cols = (n + rows - 1) / rows;
	int left = n;
	for (const Rect &row : split(r, rows, g, Axis::Y)) {
		const int k = std::min(cols, left);
		for (const Rect &c : split(row, k, g, Axis::X))
			out.push_back(c);
		left -= k;
	}
	return out;
}

/* Portrait canvas (9:16 for TikTok, Reels, Shorts): guests stack instead of sitting side by
 * side, screens keep 16:9 across the full width at the top. */
std::vector<Slot> buildPortrait(const std::string &key, const Rect &canvas, const BroadcastSpec &spec)
{
	std::vector<Slot> out;
	const Rect r = inset(canvas, spec.margin);
	const double g = std::max(0.0, spec.gap);

	if (key == "Cam2Duo169") {
		for (const Rect &half : split(r, 2, g, Axis::Y))
			out.push_back({SlotKind::Camera, fit169(half)});
		return out;
	}
	if (key.rfind("Cam", 0) == 0) {
		/* The landscape arrangement turned a quarter: columns become rows. */
		for (const Slot &s : buildLandscape(key, transposed(canvas), spec))
			out.push_back({s.kind, transposed(s.cell)});
		return out;
	}
	if (key == "Screen1Full") {
		out.push_back({SlotKind::Screen, r});
	} else if (key == "Screen2Split") {
		for (const Rect &half : split(r, 2, g, Axis::Y))
			out.push_back({SlotKind::Screen, fit169(half)});
	} else if (key == "Screen1Cam1Pip") {
		const double w = canvas.w * 0.42;
		const double h = w / k169;
		const double pad = std::max(spec.margin, canvas.w * 0.05);
		out.push_back({SlotKind::Camera, {canvas.right() - pad - w, canvas.bottom() - pad - h, w, h}});
		out.push_back({SlotKind::Screen, r});
	} else if (key == "Screen2Cam2Grid") {
		const double sh = r.w / k169;
		const Rect top{r.x, r.y, r.w, sh * 2 + g};
		const Rect rest{r.x, top.bottom() + g, r.w, r.bottom() - top.bottom() - g};
		if (rest.h > 0)
			cams(out, split(rest, 2, g, Axis::X));
		for (const Rect &s : split(top, 2, g, Axis::Y))
			out.push_back({SlotKind::Screen, s});
	} else if (key == "Screen1Cam1Side" || key.rfind("Screen1Cam", 0) == 0) {
		const bool column = key == "Screen1Cam1Side" || key.find("Column") != std::string::npos;
		const int n = key == "Screen1Cam1Side" ? 1 : std::max(1, key[10] - '0');
		const Rect screen{r.x, r.y, r.w, r.w / k169};
		const Rect rest{r.x, screen.bottom() + g, r.w, r.bottom() - screen.bottom() - g};
		if (rest.h > 0) {
			/* Column: one guest per row. Strip: guests side by side, two rows from 4. */
			cams(out, rowsOf(rest, n, column ? n : (n <= 3 ? 1 : 2), g));
		}
		out.push_back({SlotKind::Screen, screen});
	}
	return out;
}

} // namespace

std::vector<Slot> buildBroadcast(const std::string &key, const Rect &canvas, const BroadcastSpec &spec)
{
	if (!(canvas.w > 0) || !(canvas.h > 0))
		return {};
	return canvas.h > canvas.w ? buildPortrait(key, canvas, spec) : buildLandscape(key, canvas, spec);
}

CoverResult coverCell(uint32_t srcW, uint32_t srcH, double cellW, double cellH, double keepTop)
{
	CoverResult r;
	r.scale = {1, 1};
	if (srcW == 0 || srcH == 0 || !(cellW > 0) || !(cellH > 0))
		return r;
	const double srcA = (double)srcW / srcH, cellA = cellW / cellH;
	if (srcA > cellA) {
		const int keepW = std::max(1, (int)std::lround(srcH * cellA));
		const int total = (int)srcW - keepW;
		r.crop.left = total / 2;
		r.crop.right = total - r.crop.left;
	} else if (srcA < cellA) {
		const int keepH = std::max(1, (int)std::lround(srcW / cellA));
		const int total = (int)srcH - keepH;
		r.crop.top = (int)std::lround(total * std::clamp(keepTop, 0.0, 1.0));
		r.crop.bottom = total - r.crop.top;
	}
	const double cw = (double)srcW - r.crop.left - r.crop.right;
	const double ch = (double)srcH - r.crop.top - r.crop.bottom;
	r.scale = {cellW / cw, cellH / ch};
	return r;
}

} // namespace layouts::core
