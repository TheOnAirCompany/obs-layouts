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

#include "portrait-map.hpp"

#include <algorithm>
#include <cmath>

namespace layouts::core {

namespace {

constexpr double k169 = 16.0 / 9.0;
constexpr double kChatBand = 0.2;    /* share of the height for a chat panel */
constexpr double kTickerBand = 0.08; /* share of the height for a chat ticker */
constexpr double kScreenCap = 0.5;   /* screens never take more than half when guests remain */
constexpr double kHostShare = 0.4;   /* height of the host row among 3+ guests */

int kindIndex(SlotKind k)
{
	return k == SlotKind::Camera ? 0 : (k == SlotKind::Screen ? 1 : 2);
}

Rect inset(const Rect &r, double d)
{
	d = std::max(0.0, std::min(d, std::min(r.w, r.h) / 4));
	return {r.x + d, r.y + d, r.w - 2 * d, r.h - 2 * d};
}

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

Rect fit169(const Rect &r)
{
	double w = r.w, h = r.w / k169;
	if (h > r.h) {
		h = r.h;
		w = h * k169;
	}
	return {r.x + (r.w - w) / 2, r.y + (r.h - h) / 2, w, h};
}

/* n tiles in `rows` rows; the last row shares its width between what is left. */
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

/* Guests in a block: the column count whose cells are closest to square (faces), with a
 * small preference for one guest per row (the "stacked" look of vertical lives). */
std::vector<Rect> guestGrid(const Rect &r, int n, double g)
{
	std::vector<Rect> best;
	double bestScore = -1;
	for (int cols = 1; cols <= std::min(n, 3); cols++) {
		const int rows = (n + cols - 1) / cols;
		std::vector<Rect> cells = rowsOf(r, n, rows, g);
		double score = 0;
		for (const Rect &c : cells) {
			const double a = c.h > 0 ? c.w / c.h : 0;
			score += a > 0 ? std::min(a, 1.0 / a) : 0;
		}
		score /= std::max<size_t>(1, cells.size());
		if (cols == 1)
			score += 0.1;
		if (score > bestScore + 1e-9) {
			bestScore = score;
			best = std::move(cells);
		}
	}
	return best;
}

std::vector<Slot> generate(int cameras, int screens, int chats, bool tickerOnly, int host, const Rect &canvas,
			   const BroadcastSpec &spec)
{
	std::vector<Slot> out;
	const Rect r = inset(canvas, spec.margin);
	const double g = std::max(0.0, spec.gap);
	double top = r.y, bottom = r.bottom();

	/* Chat at the bottom, panels side by side. */
	std::vector<Rect> chatCells;
	if (chats > 0) {
		const double bandH = r.h * (tickerOnly ? kTickerBand : kChatBand);
		const Rect band{r.x, bottom - bandH, r.w, bandH};
		chatCells = split(band, chats, g, Axis::X);
		bottom = band.y - g;
	}

	/* Screens on top, 16:9 across the full width (two columns from three screens). */
	std::vector<Rect> screenCells;
	if (screens > 0) {
		const int cols = screens <= 2 ? 1 : 2;
		const int rows = (screens + cols - 1) / cols;
		const double cellW = (r.w - g * (cols - 1)) / cols;
		double blockH = rows * (cellW / k169) + g * (rows - 1);
		const double avail = bottom - top;
		if (cameras > 0)
			blockH = std::min(blockH, avail * kScreenCap);
		blockH = std::min(blockH, avail);
		double y = top;
		if (cameras == 0)
			y = top + (avail - blockH) / 2; /* alone: centered between top and chat */
		for (const Rect &c : rowsOf({r.x, y, r.w, blockH}, screens, rows, g))
			screenCells.push_back(fit169(c));
		top = y + blockH + g;
	}

	/* Guests in what is left. */
	std::vector<Rect> camCells;
	const Rect guests{r.x, top, r.w, std::max(0.0, bottom - top)};
	if (cameras > 0 && guests.h > 0) {
		if (host >= 0 && host < cameras && cameras >= 3) {
			const double hostH = (guests.h - g) * kHostShare;
			const Rect others{guests.x, guests.y + hostH + g, guests.w, guests.h - hostH - g};
			std::vector<Rect> rest = guestGrid(others, cameras - 1, g);
			rest.insert(rest.begin() + host, Rect{guests.x, guests.y, guests.w, hostH});
			camCells = std::move(rest);
		} else {
			camCells = guestGrid(guests, cameras, g);
		}
	}

	for (const Rect &c : camCells)
		out.push_back({SlotKind::Camera, c});
	for (const Rect &c : screenCells)
		out.push_back({SlotKind::Screen, c});
	for (const Rect &c : chatCells)
		out.push_back({SlotKind::Chat, c});
	return out;
}

} // namespace

bool supportsPortrait(const std::string &key)
{
	for (const BroadcastTemplate &t : broadcastTemplates()) {
		if (t.key == key)
			return !t.landscapeOnly && t.chats == 0;
	}
	return false;
}

PortraitMap mapToPortrait(const std::string &key, const std::vector<Slot> &landscape, const Rect &portrait,
			  const BroadcastSpec &spec)
{
	PortraitMap m;
	if (!(portrait.w > 0) || !(portrait.h > 0))
		return m;

	int counts[3] = {0, 0, 0};
	int host = -1;
	bool tickerOnly = true;
	for (const Slot &s : landscape) {
		if (s.kind == SlotKind::Camera && s.host && host < 0)
			host = counts[0];
		if (s.kind == SlotKind::Chat && !s.ticker)
			tickerOnly = false;
		counts[kindIndex(s.kind)]++;
	}

	if (supportsPortrait(key)) {
		std::vector<Slot> built = buildBroadcast(key, portrait, spec);
		int got[3] = {0, 0, 0};
		for (const Slot &s : built)
			got[kindIndex(s.kind)]++;
		if (got[0] == counts[0] && got[1] == counts[1] && got[2] == counts[2]) {
			m.strategy = PortraitStrategy::Legacy;
			m.key = key;
			/* buildBroadcast already orders cameras, then screens. */
			std::stable_sort(built.begin(), built.end(), [](const Slot &a, const Slot &b) {
				return kindIndex(a.kind) < kindIndex(b.kind);
			});
			m.slots = std::move(built);
		}
	}
	if (m.slots.empty() && !landscape.empty()) {
		m.strategy = PortraitStrategy::Generated;
		m.key = "Portrait/" + std::to_string(counts[0]) + "c" + std::to_string(counts[1]) + "s" +
			std::to_string(counts[2]) + "h";
		m.slots = generate(counts[0], counts[1], counts[2], tickerOnly, host, portrait, spec);
	}

	/* Twin links: the i-th slot of a kind goes to the i-th portrait slot of that kind. */
	int base[3] = {0, 0, 0};
	int have[3] = {0, 0, 0};
	for (const Slot &s : m.slots)
		have[kindIndex(s.kind)]++;
	base[1] = have[0];
	base[2] = have[0] + have[1];
	int seen[3] = {0, 0, 0};
	m.twinOf.assign(landscape.size(), -1);
	for (size_t i = 0; i < landscape.size(); i++) {
		const Slot &s = landscape[i];
		const int k = kindIndex(s.kind);
		const int n = seen[k]++;
		if (n >= have[k])
			continue;
		const int j = base[k] + n;
		m.twinOf[i] = j;
		Slot &t = m.slots[j];
		t.host = s.host;
		t.ticker = s.ticker;
		if (s.shape == SlotShape::Round)
			t.shape = SlotShape::Round; /* bubbles stay bubbles; diagonals and tilts do not carry */
	}
	return m;
}

PortraitMap mapTemplateToPortrait(const std::string &key, const Rect &landscape, const Rect &portrait,
				  const BroadcastSpec &spec)
{
	return mapToPortrait(key, buildBroadcast(key, landscape, spec), portrait, spec);
}

} // namespace layouts::core
