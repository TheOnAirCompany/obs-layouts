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

#include "snapping.hpp"

#include <algorithm>
#include <cmath>

namespace layouts::core {

namespace {

constexpr double kCoincide = 0.01;

Axis other(Axis a)
{
	return a == Axis::X ? Axis::Y : Axis::X;
}

struct Candidate {
	double value;
	SnapKind kind;
	double from; /* extent on the cross axis, for feedback */
	double to;
};

/* Line candidates (guides, grid, items, canvas) on one axis near [lo, hi]. */
std::vector<Candidate> lineCandidates(Axis axis, const SnapContext &ctx, const SnapSettings &s, double lo, double hi)
{
	std::vector<Candidate> c;
	const Axis ox = other(axis);
	const double cFrom = ctx.canvas.start(ox);
	const double cTo = ctx.canvas.end(ox);

	if (s.guides) {
		for (double g : axis == Axis::X ? ctx.guideX : ctx.guideY)
			c.push_back({g, SnapKind::Guide, cFrom, cTo});
	}
	if (s.grid) {
		if (ctx.square) {
			for (double v : squareLines(*ctx.square, axis, lo, hi))
				c.push_back({v, SnapKind::Grid, cFrom, cTo});
		}
		for (double v : axis == Axis::X ? ctx.trackX : ctx.trackY)
			c.push_back({v, SnapKind::Grid, cFrom, cTo});
	}
	if (s.items) {
		for (const Rect &r : ctx.others) {
			for (double v : {r.start(axis), r.mid(axis), r.end(axis)})
				c.push_back({v, SnapKind::Item, r.start(ox), r.end(ox)});
		}
	}
	if (s.canvas && ctx.canvas.w > 0 && ctx.canvas.h > 0) {
		for (double v : {ctx.canvas.start(axis), ctx.canvas.mid(axis), ctx.canvas.end(axis)})
			c.push_back({v, SnapKind::Canvas, cFrom, cTo});
	}
	return c;
}

struct SpacingCandidate {
	double start; /* proposed start of the moving rect on the axis */
	SpacingHint hint;
};

bool crossOverlap(const Rect &a, const Rect &b, Axis axis)
{
	const Axis ox = other(axis);
	return a.start(ox) < b.end(ox) && b.start(ox) < a.end(ox);
}

double overlapMid(const Rect &a, const Rect &b, Axis axis)
{
	const Axis ox = other(axis);
	const double lo = std::max(a.start(ox), b.start(ox));
	const double hi = std::min(a.end(ox), b.end(ox));
	return (lo + hi) / 2.0;
}

std::vector<SpacingCandidate> spacingCandidates(Axis axis, const Rect &moving, const SnapContext &ctx)
{
	std::vector<SpacingCandidate> out;
	std::vector<Rect> row;
	for (const Rect &r : ctx.others) {
		if (crossOverlap(r, moving, axis))
			row.push_back(r);
	}
	if (row.empty())
		return out;

	const double size = moving.size(axis);
	const double mid = moving.mid(axis);
	const Rect *before = nullptr;
	const Rect *after = nullptr;
	for (const Rect &r : row) {
		if (r.mid(axis) < mid && r.end(axis) <= moving.mid(axis)) {
			if (!before || r.end(axis) > before->end(axis))
				before = &r;
		} else if (r.mid(axis) > mid && r.start(axis) >= moving.mid(axis)) {
			if (!after || r.start(axis) < after->start(axis))
				after = &r;
		}
	}

	auto seg = [&](const Rect &a, double s, double e, const Rect &b) {
		return SpacingHint::Segment{s, e, overlapMid(a, b, axis)};
	};

	/* Centered between the two neighbours. */
	if (before && after) {
		const double room = after->start(axis) - before->end(axis);
		if (room >= size) {
			const double gap = (room - size) / 2.0;
			const double start = before->end(axis) + gap;
			const Rect placed = (axis == Axis::X) ? Rect{start, moving.y, moving.w, moving.h}
							      : Rect{moving.x, start, moving.w, moving.h};
			SpacingHint h{axis, gap, {}};
			h.segments.push_back(seg(*before, before->end(axis), start, placed));
			h.segments.push_back(seg(placed, start + size, after->start(axis), *after));
			out.push_back({start, h});
		}
	}

	/* Same gap as an existing pair of neighbours in the row. */
	std::vector<Rect> sorted = row;
	std::sort(sorted.begin(), sorted.end(),
		  [&](const Rect &a, const Rect &b) { return a.start(axis) < b.start(axis); });
	for (size_t i = 1; i < sorted.size(); i++) {
		const Rect &a = sorted[i - 1];
		const Rect &b = sorted[i];
		const double gap = b.start(axis) - a.end(axis);
		if (!(gap > 0) || !crossOverlap(a, b, axis))
			continue;
		SpacingHint::Segment ref = seg(a, a.end(axis), b.start(axis), b);
		if (before) {
			const double start = before->end(axis) + gap;
			const Rect placed = (axis == Axis::X) ? Rect{start, moving.y, moving.w, moving.h}
							      : Rect{moving.x, start, moving.w, moving.h};
			SpacingHint h{axis, gap, {ref, seg(*before, before->end(axis), start, placed)}};
			out.push_back({start, h});
		}
		if (after) {
			const double start = after->start(axis) - gap - size;
			const Rect placed = (axis == Axis::X) ? Rect{start, moving.y, moving.w, moving.h}
							      : Rect{moving.x, start, moving.w, moving.h};
			SpacingHint h{axis, gap, {ref, seg(placed, start + size, after->start(axis), *after)}};
			out.push_back({start, h});
		}
	}
	return out;
}

struct AxisSnap {
	bool snapped = false;
	double delta = 0.0;
	std::optional<SpacingHint> spacing;
};

AxisSnap snapAxis(Axis axis, const Rect &moving, const SnapContext &ctx, const SnapSettings &s, double threshold)
{
	AxisSnap best;
	int bestTier = 1000;
	double bestDist = 0.0;

	auto consider = [&](double delta, int tier, const std::optional<SpacingHint> &spacing) {
		const double dist = std::fabs(delta);
		if (dist > threshold)
			return;
		if (!best.snapped || tier < bestTier || (tier == bestTier && dist < bestDist - 1e-9)) {
			best.snapped = true;
			best.delta = delta;
			best.spacing = spacing;
			bestTier = tier;
			bestDist = dist;
		}
	};

	const double anchors[3] = {moving.start(axis), moving.mid(axis), moving.end(axis)};
	const double lo = moving.start(axis) - threshold;
	const double hi = moving.end(axis) + threshold;
	for (const Candidate &c : lineCandidates(axis, ctx, s, lo, hi)) {
		for (double a : anchors)
			consider(c.value - a, (int)c.kind, std::nullopt);
	}

	if (s.items && s.spacing) {
		for (const SpacingCandidate &sc : spacingCandidates(axis, moving, ctx))
			consider(sc.start - moving.start(axis), (int)SnapKind::Item, sc.hint);
	}
	return best;
}

void collectLines(Axis axis, const Rect &snapped, const SnapContext &ctx, const SnapSettings &s,
		  std::vector<SnapLine> &out)
{
	const Axis ox = other(axis);
	const double anchors[3] = {snapped.start(axis), snapped.mid(axis), snapped.end(axis)};
	for (const Candidate &c : lineCandidates(axis, ctx, s, snapped.start(axis) - 1, snapped.end(axis) + 1)) {
		for (double a : anchors) {
			if (std::fabs(c.value - a) <= kCoincide) {
				double from = c.from, to = c.to;
				if (c.kind == SnapKind::Item) {
					from = std::min(from, snapped.start(ox));
					to = std::max(to, snapped.end(ox));
				}
				out.push_back({axis, c.value, c.kind, from, to});
			}
		}
	}
}

} // namespace

SnapResult snapMove(const Rect &moving, const SnapContext &ctx, const SnapSettings &s, double threshold, bool allowX,
		    bool allowY)
{
	SnapResult r;
	if (!s.enabled || !(threshold > 0))
		return r;

	if (allowX) {
		const AxisSnap x = snapAxis(Axis::X, moving, ctx, s, threshold);
		if (x.snapped) {
			r.snappedX = true;
			r.delta.x = x.delta;
			if (x.spacing)
				r.spacings.push_back(*x.spacing);
		}
	}
	if (allowY) {
		const AxisSnap y = snapAxis(Axis::Y, moving, ctx, s, threshold);
		if (y.snapped) {
			r.snappedY = true;
			r.delta.y = y.delta;
			if (y.spacing)
				r.spacings.push_back(*y.spacing);
		}
	}

	const Rect snapped = moving.translated(r.delta);
	if (r.snappedX)
		collectLines(Axis::X, snapped, ctx, s, r.lines);
	if (r.snappedY)
		collectLines(Axis::Y, snapped, ctx, s, r.lines);
	return r;
}

ValueSnap snapValue(double v, Axis axis, const SnapContext &ctx, const SnapSettings &s, double threshold,
		    double crossFrom, double crossTo)
{
	ValueSnap r;
	r.value = v;
	if (!s.enabled || !(threshold > 0))
		return r;

	int bestTier = 1000;
	double bestDist = 0.0;
	for (const Candidate &c : lineCandidates(axis, ctx, s, v - threshold, v + threshold)) {
		const double dist = std::fabs(c.value - v);
		if (dist > threshold)
			continue;
		if (!r.snapped || (int)c.kind < bestTier || ((int)c.kind == bestTier && dist < bestDist - 1e-9)) {
			r.snapped = true;
			r.value = c.value;
			bestTier = (int)c.kind;
			bestDist = dist;
			double from = c.from, to = c.to;
			if (c.kind == SnapKind::Item) {
				from = std::min(from, crossFrom);
				to = std::max(to, crossTo);
			}
			r.line = SnapLine{axis, c.value, c.kind, from, to};
		}
	}
	return r;
}

std::vector<Measure> measureBetween(const Rect &a, const Rect &b)
{
	std::vector<Measure> out;
	for (Axis axis : {Axis::X, Axis::Y}) {
		const Axis ox = other(axis);
		const bool overlapCross = a.start(ox) < b.end(ox) && b.start(ox) < a.end(ox);
		const double cross = overlapCross ? overlapMid(a, b, axis) : a.mid(ox);

		if (b.start(axis) >= a.end(axis)) {
			out.push_back({axis, a.end(axis), b.start(axis), cross});
		} else if (b.end(axis) <= a.start(axis)) {
			out.push_back({axis, b.end(axis), a.start(axis), cross});
		} else {
			/* Overlapping on this axis: show edge-to-edge distances when one range
			 * contains the other (a inside b, or b inside a). */
			const bool aInB = a.start(axis) >= b.start(axis) && a.end(axis) <= b.end(axis);
			const bool bInA = b.start(axis) >= a.start(axis) && b.end(axis) <= a.end(axis);
			if (aInB || bInA) {
				const double c = aInB ? a.mid(ox) : b.mid(ox);
				const double s0 = std::min(a.start(axis), b.start(axis));
				const double s1 = std::max(a.start(axis), b.start(axis));
				const double e0 = std::min(a.end(axis), b.end(axis));
				const double e1 = std::max(a.end(axis), b.end(axis));
				if (s1 - s0 > kCoincide)
					out.push_back({axis, s0, s1, c});
				if (e1 - e0 > kCoincide)
					out.push_back({axis, e0, e1, c});
			}
		}
	}
	return out;
}

} // namespace layouts::core
