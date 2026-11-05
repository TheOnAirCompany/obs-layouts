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

#include "broadcast-screens.hpp"

#include <algorithm>
#include <functional>
#include <map>

namespace layouts::core {

namespace {

/* Design board: every template is drawn on 1920 x 1080. */
constexpr double W = 1920, H = 1080;
constexpr double A = 16.0 / 9.0;
const Rect kFull{0, 0, W, H};

enum class K { Host, Cam, Screen };

struct DSlot {
	K kind;
	Rect r;
	bool round = false;
};

using Slots = std::vector<DSlot>;
using Rects = std::vector<Rect>;

Rect inset(const Rect &r, double m)
{
	return {r.x + m, r.y + m, r.w - 2 * m, r.h - 2 * m};
}

/* Largest rect of aspect a inside r, centered. */
Rect fit(double a, const Rect &r)
{
	double w = r.w, h = w / a;
	if (h > r.h) {
		h = r.h;
		w = h * a;
	}
	return {r.x + (r.w - w) / 2, r.y + (r.h - h) / 2, w, h};
}

Rects split(const Rect &r, int n, Axis axis, double g)
{
	Rects out;
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

Rects grid(const Rect &r, int cols, int rows, double g)
{
	Rects out;
	for (const Rect &row : split(r, rows, Axis::Y, g))
		for (const Rect &c : split(row, cols, Axis::X, g))
			out.push_back(c);
	return out;
}

Rects fitAll(double a, const Rects &cells)
{
	Rects out;
	for (const Rect &c : cells)
		out.push_back(fit(a, c));
	return out;
}

/* n tiles of aspect a in one centered row (or column) inside r. */
Rects rowOf(int n, double a, const Rect &r, double g)
{
	double w = (r.w - g * (n - 1)) / n, h = w / a;
	if (h > r.h) {
		h = r.h;
		w = h * a;
	}
	const double tw = n * w + (n - 1) * g;
	Rects out;
	for (int i = 0; i < n; i++)
		out.push_back({r.x + (r.w - tw) / 2 + i * (w + g), r.y + (r.h - h) / 2, w, h});
	return out;
}

Rects colOf(int n, double a, const Rect &r, double g)
{
	double h = (r.h - g * (n - 1)) / n, w = h * a;
	if (w > r.w) {
		w = r.w;
		h = w / a;
	}
	const double th = n * h + (n - 1) * g;
	Rects out;
	for (int i = 0; i < n; i++)
		out.push_back({r.x + (r.w - w) / 2, r.y + (r.h - th) / 2 + i * (h + g), w, h});
	return out;
}

/* Rows of tiles of aspect a (count per row), centered as a block. */
Rects rowsOf(const std::vector<int> &counts, double a, const Rect &r, double g)
{
	const int rows = (int)counts.size();
	const int maxN = *std::max_element(counts.begin(), counts.end());
	double w = (r.w - g * (maxN - 1)) / maxN, h = w / a;
	if (rows * h + (rows - 1) * g > r.h) {
		h = (r.h - g * (rows - 1)) / rows;
		w = h * a;
	}
	const double th = rows * h + (rows - 1) * g;
	Rects out;
	for (int i = 0; i < rows; i++) {
		const int n = counts[i];
		const double tw = n * w + (n - 1) * g;
		for (int k = 0; k < n; k++)
			out.push_back({r.x + (r.w - tw) / 2 + k * (w + g), r.y + (r.h - th) / 2 + i * (h + g), w, h});
	}
	return out;
}

/* Inset in the bottom-right corner, 16:9. */
Rect pip(double w, double m = 40)
{
	const double h = w / A;
	return {W - m - w, H - m - h, w, h};
}

/* Cameras: the first one is the host. */
void cams(Slots &out, const Rects &rs)
{
	for (const Rect &r : rs)
		out.push_back({out.empty() ? K::Host : K::Cam, r});
}

void scr(Slots &out, const Rects &rs)
{
	for (const Rect &r : rs)
		out.push_back({K::Screen, r});
}

/* `G` is the spec gap in design units. */
using Builder = std::function<void(Slots &, double G)>;

struct Entry {
	const char *key;
	int cameras;
	int screens;
	Builder build;
};

/* Column of n cams on each side of a centered screen. */
void sides(Slots &s, int n)
{
	Rects c = colOf(n, A, {20, 0, 364, H}, 16);
	const Rects right = colOf(n, A, {1536, 0, 364, H}, 16);
	c.insert(c.end(), right.begin(), right.end());
	cams(s, c);
	scr(s, {fit(A, {400, 0, 1120, H})});
}

/* Screen top left, two cams on its right, n cams in a band under it. */
void lShape(Slots &s, double G, int bottom)
{
	const Rect screen{0, 0, 1424, 801};
	const double c = W - 1424 - G, h = (801 - G) / 2;
	Rects cells{{1424 + G, 0, c, h}, {1424 + G, h + G, c, h}};
	if (bottom == 2) {
		const double bw = (1424 - G) / 2;
		cells.push_back({0, 801 + G, bw, H - 801 - G});
		cells.push_back({(1424 + G) / 2, 801 + G, bw, H - 801 - G});
	} else {
		for (const Rect &r : split({0, 801 + G, W, H - 801 - G}, bottom, Axis::X, G))
			cells.push_back(r);
	}
	cams(s, cells);
	scr(s, {screen});
}

const std::vector<Entry> &entries()
{
	static const std::vector<Entry> list = {
		/* Validated on the 0.5 review page. */
		{"Screen1Cam1Float", 1, 1,
		 [](Slots &s, double) {
			 cams(s, {{1530, 615, 330, 330}});
			 scr(s, {{60, 135, 1440, 810}});
		 }},
		{"Screen1Cam2Pip", 2, 1,
		 [](Slots &s, double) {
			 cams(s, {{1440, 490, 440, 248}, {1440, 762, 440, 248}});
			 scr(s, {kFull});
		 }},
		{"Screen1Cam2Sides", 2, 1,
		 [](Slots &s, double) {
			 cams(s, {{20, 220, 360, 640}, {1540, 220, 360, 640}});
			 scr(s, {{400, 225, 1120, 630}});
		 }},
		{"Screen1Cam1Half", 1, 1,
		 [](Slots &s, double) {
			 cams(s, {{980, 281, 900, 518}});
			 scr(s, {{40, 281, 900, 518}});
		 }},
		{"Screen3", 0, 3,
		 [](Slots &s, double) { scr(s, {{0, 0, 1280, 720}, {1288, 0, 632, 356}, {1288, 364, 632, 356}}); }},
		{"Screen4Grid", 0, 4, [](Slots &s, double) { scr(s, fitAll(A, grid(kFull, 2, 2, 8))); }},

		/* Screens alone. */
		{"Screen1Framed", 0, 1, [](Slots &s, double) { scr(s, {fit(A, inset(kFull, 90))}); }},
		{"Screen2Stacked", 0, 2, [](Slots &s, double) { scr(s, colOf(2, A, inset(kFull, 30), 20)); }},
		{"Screen2BigSmall", 0, 2,
		 [](Slots &s, double G) { scr(s, {fit(A, {0, 0, 1400, H}), fit(A, {1400 + G, 0, W - 1400 - G, H})}); }},
		{"Screen2Pip", 0, 2, [](Slots &s, double) { scr(s, {kFull, pip(560)}); }},
		{"Screen3Row", 0, 3, [](Slots &s, double) { scr(s, rowOf(3, A, inset(kFull, 20), 16)); }},
		{"Screen3TwoOne", 0, 3, [](Slots &s, double) { scr(s, rowsOf({2, 1}, A, inset(kFull, 20), 16)); }},
		{"Screen4Strip", 0, 4,
		 [](Slots &s, double G) {
			 scr(s, {fit(A, {0, 0, W, 780})});
			 scr(s, rowOf(3, A, {0, 780 + G, W, H - 780 - G}, 16));
		 }},
		{"Screen4Column", 0, 4,
		 [](Slots &s, double G) {
			 const double sw = (W - G) * 0.75;
			 const Rect main = fit(A, {0, 0, sw, H});
			 scr(s, {main});
			 scr(s, split({sw + G, main.y, W - sw - G, main.h}, 3, Axis::Y, G));
		 }},

		/* One screen + cameras. */
		{"Screen1Cam1Portrait", 1, 1,
		 [](Slots &s, double G) {
			 const double cw = H * 9 / 16;
			 cams(s, {{W - cw, 0, cw, H}});
			 scr(s, {fit(A, {0, 0, W - cw - G, H})});
		 }},
		{"Screen1Cam1Bubble", 1, 1,
		 [](Slots &s, double) {
			 s.push_back({K::Host, {W - 40 - 300, H - 40 - 300, 300, 300}, true});
			 scr(s, {kFull});
		 }},
		{"Screen1Cam1Over", 1, 1,
		 [](Slots &s, double) {
			 cams(s, {kFull});
			 scr(s, {{760, 90, 1100, 619}});
		 }},
		{"Screen1Cam1Stack", 1, 1,
		 [](Slots &s, double) {
			 const Rects c = colOf(2, A, inset(kFull, 30), 20);
			 cams(s, {c[1]});
			 scr(s, {c[0]});
		 }},
		{"Screen1Cam2Square", 2, 1,
		 [](Slots &s, double) {
			 cams(s, colOf(2, 1, {1470, 60, 390, 960}, 24));
			 scr(s, {{60, 135, 1380, 776}});
		 }},
		{"Screen1Cam2HostGuest", 2, 1,
		 [](Slots &s, double G) {
			 const double sw = (W - G) * 0.7;
			 const Rect screen = fit(A, {0, 0, sw, H});
			 const double x = sw + G, w = W - x;
			 cams(s, {{x, screen.y, w, screen.h * 0.6 - G / 2},
				  {x, screen.y + screen.h * 0.6 + G / 2, w, screen.h * 0.4 - G / 2}});
			 scr(s, {screen});
		 }},
		{"Screen1Cam3Pip", 3, 1,
		 [](Slots &s, double) {
			 cams(s, colOf(3, A, {1500, 40, 380, 1000}, 16));
			 scr(s, {kFull});
		 }},
		{"Screen1Cam3Float", 3, 1,
		 [](Slots &s, double) {
			 cams(s, colOf(3, 1, {1550, 40, 320, 1000}, 20));
			 scr(s, {{60, 182, 1460, 821}});
		 }},
		{"Screen1Cam4Pip", 4, 1,
		 [](Slots &s, double) {
			 cams(s, rowOf(4, A, {40, 830, W - 80, 210}, 16));
			 scr(s, {kFull});
		 }},
		{"Screen1Cam4Grid", 4, 1,
		 [](Slots &s, double G) {
			 const Rects halves = split(kFull, 2, Axis::X, G);
			 cams(s, grid(fit(A, halves[1]), 2, 2, G));
			 scr(s, {fit(A, halves[0])});
		 }},
		{"Screen1Cam4Sides", 4, 1, [](Slots &s, double) { sides(s, 2); }},
		{"Screen1Cam4L", 4, 1, [](Slots &s, double G) { lShape(s, G, 2); }},
		{"Screen1Cam5L", 5, 1, [](Slots &s, double G) { lShape(s, G, 3); }},
		{"Screen1Cam6L", 6, 1, [](Slots &s, double G) { lShape(s, G, 4); }},
		{"Screen1Cam6Sides", 6, 1, [](Slots &s, double) { sides(s, 3); }},
		{"Screen1Cam6Grid", 6, 1,
		 [](Slots &s, double G) {
			 const double sw = (W - G) * 0.55;
			 const Rect screen = fit(A, {0, 0, sw, H});
			 cams(s, grid({sw + G, screen.y, W - sw - G, screen.h}, 2, 3, G));
			 scr(s, {screen});
		 }},

		/* Two screens + cameras. */
		{"Screen2Cam1", 1, 2,
		 [](Slots &s, double G) {
			 cams(s, {fit(A, {0, 540 + G, W, H - 540 - G})});
			 scr(s, fitAll(A, split({0, 0, W, 540}, 2, Axis::X, G)));
		 }},
		{"Screen2Cam1Pip", 1, 2,
		 [](Slots &s, double G) {
			 cams(s, {{720, 800, 480, 270}});
			 scr(s, fitAll(A, split(kFull, 2, Axis::X, G)));
		 }},
		{"Screen2Cam1Side", 1, 2,
		 [](Slots &s, double G) {
			 cams(s, {fit(A, {960 + G, 0, W - 960 - G, H})});
			 scr(s, colOf(2, A, {0, 0, 960, H}, 16));
		 }},
		{"Screen2Cam2Pip", 2, 2,
		 [](Slots &s, double G) {
			 const Rects sc = fitAll(A, split(kFull, 2, Axis::X, G));
			 Rects c;
			 for (const Rect &r : sc)
				 c.push_back({r.right() - 300, r.bottom() - 185, 280, 158});
			 cams(s, c);
			 scr(s, sc);
		 }},
		{"Screen2Cam2Sides", 2, 2,
		 [](Slots &s, double) {
			 cams(s, {fit(9.0 / 16, {20, 0, 364, H}), fit(9.0 / 16, {1536, 0, 364, H})});
			 scr(s, colOf(2, A, {400, 20, 1120, 1040}, 16));
		 }},
		{"Screen2Cam3", 3, 2,
		 [](Slots &s, double G) {
			 cams(s, split({0, 700 + G, W, H - 700 - G}, 3, Axis::X, G));
			 scr(s, fitAll(A, split({0, 0, W, 700}, 2, Axis::X, G)));
		 }},
		{"Screen2Cam4", 4, 2,
		 [](Slots &s, double G) {
			 cams(s, split({0, 760 + G, W, H - 760 - G}, 4, Axis::X, G));
			 scr(s, fitAll(A, split({0, 0, W, 760}, 2, Axis::X, G)));
		 }},

		/* Three or four screens + cameras. */
		{"Screen3Cam1", 1, 3,
		 [](Slots &s, double) {
			 const Rects c = fitAll(A, grid(kFull, 2, 2, 8));
			 cams(s, {c[3]});
			 scr(s, {c[0], c[1], c[2]});
		 }},
		{"Screen3Cam2", 2, 3,
		 [](Slots &s, double G) {
			 cams(s, rowOf(2, A, {0, 560 + G, W, H - 560 - G}, 16));
			 scr(s, rowOf(3, A, {0, 0, W, 560}, 16));
		 }},
		{"Screen3Cam3", 3, 3,
		 [](Slots &s, double G) {
			 cams(s, rowOf(3, A, {0, 540 + G, W, H - 540 - G}, 16));
			 scr(s, rowOf(3, A, {0, 0, W, 540}, 16));
		 }},
		{"Screen4Cam1", 1, 4,
		 [](Slots &s, double G) {
			 cams(s, {fit(9.0 / 16, {1440 + G, 0, W - 1440 - G, H})});
			 scr(s, grid({0, 0, 1440, 810}, 2, 2, 8));
		 }},
		{"Screen4Cam2", 2, 4,
		 [](Slots &s, double G) {
			 cams(s, colOf(2, A, {1440 + G, 0, W - 1440 - G, H}, 16));
			 scr(s, grid(fit(A, {0, 0, 1440, H}), 2, 2, 8));
		 }},
	};
	return list;
}

const Entry *find(const std::string &key)
{
	static const std::map<std::string, const Entry *> index = [] {
		std::map<std::string, const Entry *> m;
		for (const Entry &e : entries())
			m[e.key] = &e;
		return m;
	}();
	const auto it = index.find(key);
	return it == index.end() ? nullptr : it->second;
}

bool overlap(const Rect &a, const Rect &b)
{
	constexpr double eps = 1e-6;
	return a.x < b.right() - eps && b.x < a.right() - eps && a.y < b.bottom() - eps && b.y < a.bottom() - eps;
}

} // namespace

void appendScreenTemplates(std::vector<BroadcastTemplate> &list)
{
	for (const Entry &e : entries()) {
		BroadcastTemplate t{e.key, e.cameras, e.screens};
		t.landscapeOnly = true;
		list.push_back(t);
	}
}

bool buildScreenTemplate(const std::string &key, const Rect &canvas, const BroadcastSpec &spec, std::vector<Slot> &out)
{
	const Entry *e = find(key);
	if (!e)
		return false;
	if (!(canvas.w > 0) || !(canvas.h > 0))
		return true;
	/* The design board, scaled uniformly into the canvas minus the margin and centered. */
	const double m = std::max(0.0, std::min(spec.margin, std::min(canvas.w, canvas.h) / 4));
	const Rect r{canvas.x + m, canvas.y + m, canvas.w - 2 * m, canvas.h - 2 * m};
	const double k = std::min(r.w / W, r.h / H);
	const double ox = r.x + (r.w - W * k) / 2, oy = r.y + (r.h - H * k) / 2;

	Slots design;
	e->build(design, std::max(0.0, spec.gap) / k);
	/* Cameras first (host first), then screens. */
	std::stable_partition(design.begin(), design.end(), [](const DSlot &d) { return d.kind != K::Screen; });
	for (const DSlot &d : design) {
		Slot s{d.kind == K::Screen ? SlotKind::Screen : SlotKind::Camera,
		       {ox + d.r.x * k, oy + d.r.y * k, d.r.w * k, d.r.h * k}};
		s.host = d.kind == K::Host;
		if (d.round)
			s.shape = SlotShape::Round;
		out.push_back(s);
	}
	return true;
}

std::vector<size_t> overlayOrder(const std::vector<Rect> &cells)
{
	std::vector<size_t> out;
	for (size_t i = 0; i < cells.size(); i++) {
		const double area = cells[i].w * cells[i].h;
		for (size_t j = 0; j < cells.size(); j++) {
			if (j != i && cells[j].w * cells[j].h > area && overlap(cells[i], cells[j])) {
				out.push_back(i);
				break;
			}
		}
	}
	std::stable_sort(out.begin(), out.end(),
			 [&](size_t a, size_t b) { return cells[a].w * cells[a].h > cells[b].w * cells[b].h; });
	return out;
}

} // namespace layouts::core
