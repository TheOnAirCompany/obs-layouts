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

#include "arrange.hpp"

#include <algorithm>
#include <numeric>

namespace layouts::core {

namespace {

std::vector<size_t> movableSortedBy(const std::vector<ArrangeItem> &items, Axis axis, bool byCenter)
{
	std::vector<size_t> idx;
	for (size_t i = 0; i < items.size(); i++) {
		if (!items[i].locked)
			idx.push_back(i);
	}
	/* Stable: equal keys keep selection order, so results are deterministic. */
	std::stable_sort(idx.begin(), idx.end(), [&](size_t a, size_t b) {
		const double ka = byCenter ? items[a].box.mid(axis) : items[a].box.start(axis);
		const double kb = byCenter ? items[b].box.mid(axis) : items[b].box.start(axis);
		return ka < kb;
	});
	return idx;
}

Vec2 axisDelta(Axis axis, double d)
{
	return axis == Axis::X ? Vec2{d, 0.0} : Vec2{0.0, d};
}

} // namespace

Rect selectionBounds(const std::vector<ArrangeItem> &items)
{
	if (items.empty())
		return {};
	Rect r = items[0].box;
	for (const ArrangeItem &it : items)
		r = r.united(it.box);
	return r;
}

int movableCount(const std::vector<ArrangeItem> &items)
{
	return (int)std::count_if(items.begin(), items.end(), [](const ArrangeItem &i) { return !i.locked; });
}

std::optional<Rect> alignReference(const std::vector<ArrangeItem> &items, AlignTarget target, const Rect &canvas,
				   int keyIndex)
{
	switch (target) {
	case AlignTarget::Canvas:
		return canvas;
	case AlignTarget::KeyObject:
		if (keyIndex < 0 || keyIndex >= (int)items.size())
			return std::nullopt;
		return items[(size_t)keyIndex].box;
	case AlignTarget::Selection:
	default:
		if (items.empty())
			return std::nullopt;
		return selectionBounds(items);
	}
}

std::vector<Vec2> align(const std::vector<ArrangeItem> &items, AlignEdge edge, const Rect &ref, int keyIndex)
{
	std::vector<Vec2> out(items.size());
	for (size_t i = 0; i < items.size(); i++) {
		if (items[i].locked || (int)i == keyIndex)
			continue;
		const Rect &b = items[i].box;
		switch (edge) {
		case AlignEdge::Left:
			out[i] = {ref.left() - b.left(), 0};
			break;
		case AlignEdge::HCenter:
			out[i] = {ref.centerX() - b.centerX(), 0};
			break;
		case AlignEdge::Right:
			out[i] = {ref.right() - b.right(), 0};
			break;
		case AlignEdge::Top:
			out[i] = {0, ref.top() - b.top()};
			break;
		case AlignEdge::VCenter:
			out[i] = {0, ref.centerY() - b.centerY()};
			break;
		case AlignEdge::Bottom:
			out[i] = {0, ref.bottom() - b.bottom()};
			break;
		}
	}
	return out;
}

std::vector<Vec2> distributeCenters(const std::vector<ArrangeItem> &items, Axis axis)
{
	std::vector<Vec2> out(items.size());
	const std::vector<size_t> idx = movableSortedBy(items, axis, true);
	if (idx.size() < 3)
		return out;

	const double first = items[idx.front()].box.mid(axis);
	const double last = items[idx.back()].box.mid(axis);
	const double step = (last - first) / (double)(idx.size() - 1);
	for (size_t k = 1; k + 1 < idx.size(); k++) {
		const double target = first + step * (double)k;
		out[idx[k]] = axisDelta(axis, target - items[idx[k]].box.mid(axis));
	}
	return out;
}

std::vector<Vec2> distributeSpacing(const std::vector<ArrangeItem> &items, Axis axis)
{
	std::vector<Vec2> out(items.size());
	const std::vector<size_t> idx = movableSortedBy(items, axis, false);
	if (idx.size() < 3)
		return out;

	const double start = items[idx.front()].box.start(axis);
	double total = 0.0;
	for (size_t i : idx)
		total += items[i].box.size(axis);
	/* The last item (by start edge) keeps its position: span is measured to its end. */
	const double lastEnd = items[idx.back()].box.end(axis);
	const double gap = (lastEnd - start - total) / (double)(idx.size() - 1);

	double cursor = items[idx.front()].box.end(axis) + gap;
	for (size_t k = 1; k + 1 < idx.size(); k++) {
		const Rect &b = items[idx[k]].box;
		out[idx[k]] = axisDelta(axis, cursor - b.start(axis));
		cursor += b.size(axis) + gap;
	}
	return out;
}

std::vector<Vec2> spaceExactly(const std::vector<ArrangeItem> &items, Axis axis, double gap)
{
	std::vector<Vec2> out(items.size());
	const std::vector<size_t> idx = movableSortedBy(items, axis, false);
	if (idx.size() < 2 || !std::isfinite(gap))
		return out;

	double cursor = items[idx.front()].box.end(axis) + gap;
	for (size_t k = 1; k < idx.size(); k++) {
		const Rect &b = items[idx[k]].box;
		out[idx[k]] = axisDelta(axis, cursor - b.start(axis));
		cursor += b.size(axis) + gap;
	}
	return out;
}

std::optional<double> uniformGap(const std::vector<ArrangeItem> &items, Axis axis, double eps)
{
	std::vector<size_t> idx(items.size());
	std::iota(idx.begin(), idx.end(), 0);
	std::stable_sort(idx.begin(), idx.end(),
			 [&](size_t a, size_t b) { return items[a].box.start(axis) < items[b].box.start(axis); });
	if (idx.size() < 2)
		return std::nullopt;

	std::optional<double> gap;
	for (size_t k = 1; k < idx.size(); k++) {
		const double g = items[idx[k]].box.start(axis) - items[idx[k - 1]].box.end(axis);
		if (!gap)
			gap = g;
		else if (std::fabs(*gap - g) > eps)
			return std::nullopt;
	}
	return gap;
}

} // namespace layouts::core
