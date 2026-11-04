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

#include "layout.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace layouts::core {

namespace {

double alignOffset(FlowAlign a, double room)
{
	switch (a) {
	case FlowAlign::Center:
		return room / 2.0;
	case FlowAlign::End:
		return room;
	case FlowAlign::Start:
	default:
		return 0.0;
	}
}

Rect inset(const Rect &r, double d)
{
	const double dx = std::min(d, r.w / 2), dy = std::min(d, r.h / 2);
	return {r.x + dx, r.y + dy, r.w - 2 * dx, r.h - 2 * dy};
}

} // namespace

std::vector<Frame> scaleFrames(const std::vector<Frame> &frames, const Rect &from, const Rect &to)
{
	std::vector<Frame> out = frames;
	if (!(from.w > 0) || !(from.h > 0))
		return out;
	const double sx = to.w / from.w, sy = to.h / from.h;
	for (Frame &f : out) {
		f.center = {to.x + (f.center.x - from.x) * sx, to.y + (f.center.y - from.y) * sy};
		f.width = std::fabs(f.width * sx);
		f.height = std::fabs(f.height * sy);
	}
	return out;
}

Frame fitInCell(double aspect, const Rect &cell, FitMode mode)
{
	if (mode == FitMode::Stretch || !(aspect > 0) || !(cell.w > 0) || !(cell.h > 0))
		return Frame::fromRect(cell);
	const double cellAspect = cell.w / cell.h;
	const bool byWidth = (mode == FitMode::Fit) ? (aspect >= cellAspect) : (aspect < cellAspect);
	Frame f;
	f.center = cell.center();
	if (byWidth) {
		f.width = cell.w;
		f.height = cell.w / aspect;
	} else {
		f.height = cell.h;
		f.width = cell.h * aspect;
	}
	return f;
}

std::vector<size_t> readingOrder(const std::vector<Frame> &frames, FlowDirection direction)
{
	std::vector<size_t> idx(frames.size());
	std::iota(idx.begin(), idx.end(), 0);
	if (direction == FlowDirection::Horizontal) {
		std::stable_sort(idx.begin(), idx.end(),
				 [&](size_t a, size_t b) { return frames[a].center.x < frames[b].center.x; });
		return idx;
	}
	if (direction == FlowDirection::Vertical) {
		std::stable_sort(idx.begin(), idx.end(),
				 [&](size_t a, size_t b) { return frames[a].center.y < frames[b].center.y; });
		return idx;
	}
	/* Grid: rows by y with a tolerance, then x. */
	double minH = 1e300;
	for (const Frame &f : frames)
		minH = std::min(minH, f.height);
	const double tol = frames.empty() ? 0 : std::max(1.0, minH / 2);
	std::stable_sort(idx.begin(), idx.end(), [&](size_t a, size_t b) {
		const double dy = frames[a].rect().top() - frames[b].rect().top();
		if (std::fabs(dy) > tol)
			return dy < 0;
		return frames[a].center.x < frames[b].center.x;
	});
	return idx;
}

std::vector<Frame> flow(const std::vector<Frame> &frames, const Rect &container, const FlowSpec &spec)
{
	std::vector<Frame> out = frames;
	const size_t n = frames.size();
	if (n == 0)
		return out;
	const Rect inner = inset(container, std::max(0.0, spec.padding));
	const double gap = spec.gap;
	const std::vector<size_t> order = readingOrder(frames, spec.direction);

	if (spec.direction == FlowDirection::Grid) {
		const int cols = std::max(1, std::min(spec.columns, (int)n));
		const int rows = (int)((n + cols - 1) / cols);
		const double cellW = (inner.w - gap * (cols - 1)) / cols;

		/* Row heights: Fill makes square-ish cells from the width, Keep uses the
		 * tallest item of the row. */
		std::vector<double> rowH(rows, 0.0);
		std::vector<Frame> sized = frames;
		for (size_t k = 0; k < n; k++) {
			const size_t i = order[k];
			if (spec.sizing == FlowSizing::Fill && cellW > 0) {
				sized[i].width = cellW;
				sized[i].height = cellW / frames[i].aspect();
			}
			rowH[k / cols] = std::max(rowH[k / cols], sized[i].height);
		}
		double totalH = gap * (rows - 1);
		for (double h : rowH)
			totalH += h;
		double y = inner.y + alignOffset(spec.mainAlign, inner.h - totalH);
		for (int r = 0; r < rows; r++) {
			for (int c = 0; c < cols; c++) {
				const size_t k = (size_t)r * cols + c;
				if (k >= n)
					break;
				const size_t i = order[k];
				const double cellX = inner.x + c * (cellW + gap);
				Frame f = sized[i];
				const double x = cellX + alignOffset(spec.crossAlign, cellW - f.width);
				const double yy = y + alignOffset(spec.crossAlign, rowH[r] - f.height);
				f.center = {x + f.width / 2, yy + f.height / 2};
				out[i] = f;
			}
			y += rowH[r] + gap;
		}
		return out;
	}

	const bool horiz = spec.direction == FlowDirection::Horizontal;
	const double mainLen = horiz ? inner.w : inner.h;
	const double crossLen = horiz ? inner.h : inner.w;

	std::vector<Frame> sized = frames;
	if (spec.sizing == FlowSizing::Fill) {
		const double share = (mainLen - gap * (n - 1)) / n;
		if (share > 0) {
			for (size_t i = 0; i < n; i++) {
				if (horiz) {
					sized[i].width = share;
					sized[i].height = share / frames[i].aspect();
				} else {
					sized[i].height = share;
					sized[i].width = share * frames[i].aspect();
				}
			}
		}
	}

	double total = gap * (n - 1);
	for (const Frame &f : sized)
		total += horiz ? f.width : f.height;
	double cursor = (horiz ? inner.x : inner.y) + alignOffset(spec.mainAlign, mainLen - total);
	for (size_t i : order) {
		Frame f = sized[i];
		const double mainSize = horiz ? f.width : f.height;
		const double crossSize = horiz ? f.height : f.width;
		const double crossStart =
			(horiz ? inner.y : inner.x) + alignOffset(spec.crossAlign, crossLen - crossSize);
		if (horiz)
			f.center = {cursor + mainSize / 2, crossStart + crossSize / 2};
		else
			f.center = {crossStart + crossSize / 2, cursor + mainSize / 2};
		out[i] = f;
		cursor += mainSize + gap;
	}
	return out;
}

const std::vector<PresetInfo> &presets()
{
	static const std::vector<PresetInfo> list = {
		{Preset::CornerTopLeft, "CornerTopLeft", 1, 1},
		{Preset::CornerTopRight, "CornerTopRight", 1, 1},
		{Preset::CornerBottomLeft, "CornerBottomLeft", 1, 1},
		{Preset::CornerBottomRight, "CornerBottomRight", 1, 1},
		{Preset::Centered, "Centered", 1, 1},
		{Preset::FullScreen, "FullScreen", 1, 1},
		{Preset::LowerThird, "LowerThird", 1, 1},
		{Preset::PictureInPicture, "PictureInPicture", 2, 2},
		{Preset::SplitScreen, "SplitScreen", 2, 2},
		{Preset::VerticalStack, "VerticalStack", 2, 2},
		{Preset::OneAndTwo, "OneAndTwo", 3, 3},
		{Preset::GridTwoByTwo, "GridTwoByTwo", 4, 4},
	};
	return list;
}

std::optional<Preset> presetFromKey(const std::string &key)
{
	for (const PresetInfo &p : presets()) {
		if (key == p.key)
			return p.preset;
	}
	return std::nullopt;
}

std::vector<Frame> applyPreset(Preset preset, const std::vector<Frame> &frames, const Rect &canvas,
			       const PresetSpec &spec)
{
	std::vector<Frame> out = frames;
	if (frames.empty() || !(canvas.w > 0) || !(canvas.h > 0))
		return out;
	const double m = std::max(0.0, spec.margin);
	const double g = std::max(0.0, spec.gap);
	const Rect inner = inset(canvas, m);

	auto corner = [&](bool right, bool bottom) {
		const Frame &f = frames[0];
		const double w = canvas.w * std::clamp(spec.cornerScale, 0.05, 1.0);
		const double h = w / f.aspect();
		const double x = right ? canvas.right() - m - w : canvas.x + m;
		const double y = bottom ? canvas.bottom() - m - h : canvas.y + m;
		out[0] = {{x + w / 2, y + h / 2}, w, h};
	};

	switch (preset) {
	case Preset::CornerTopLeft:
		corner(false, false);
		break;
	case Preset::CornerTopRight:
		corner(true, false);
		break;
	case Preset::CornerBottomLeft:
		corner(false, true);
		break;
	case Preset::CornerBottomRight:
		corner(true, true);
		break;
	case Preset::Centered:
		out[0] = fitInCell(frames[0].aspect(), inner, FitMode::Fit);
		break;
	case Preset::FullScreen:
		out[0] = fitInCell(frames[0].aspect(), canvas, FitMode::Fill);
		break;
	case Preset::LowerThird: {
		const double w = canvas.w * 0.6;
		const double h = std::min(w / frames[0].aspect(), canvas.h * 0.25);
		const double ww = h * frames[0].aspect();
		out[0] = {{canvas.x + m + ww / 2, canvas.bottom() - m - h / 2}, ww, h};
		break;
	}
	case Preset::PictureInPicture:
		if (frames.size() >= 2) {
			out[0] = fitInCell(frames[0].aspect(), canvas, FitMode::Fill);
			const double w = canvas.w * std::clamp(spec.cornerScale, 0.05, 1.0);
			const double h = w / frames[1].aspect();
			out[1] = {{canvas.right() - m - w / 2, canvas.bottom() - m - h / 2}, w, h};
		}
		break;
	case Preset::SplitScreen:
		if (frames.size() >= 2) {
			const double cw = (inner.w - g) / 2;
			out[0] = fitInCell(frames[0].aspect(), {inner.x, inner.y, cw, inner.h}, FitMode::Fit);
			out[1] = fitInCell(frames[1].aspect(), {inner.x + cw + g, inner.y, cw, inner.h}, FitMode::Fit);
		}
		break;
	case Preset::VerticalStack:
		if (frames.size() >= 2) {
			const double ch = (inner.h - g) / 2;
			out[0] = fitInCell(frames[0].aspect(), {inner.x, inner.y, inner.w, ch}, FitMode::Fit);
			out[1] = fitInCell(frames[1].aspect(), {inner.x, inner.y + ch + g, inner.w, ch}, FitMode::Fit);
		}
		break;
	case Preset::OneAndTwo:
		if (frames.size() >= 3) {
			const double bigW = (inner.w - g) * 2.0 / 3.0;
			const double smallW = inner.w - g - bigW;
			const double smallH = (inner.h - g) / 2;
			out[0] = fitInCell(frames[0].aspect(), {inner.x, inner.y, bigW, inner.h}, FitMode::Fit);
			out[1] = fitInCell(frames[1].aspect(), {inner.x + bigW + g, inner.y, smallW, smallH},
					   FitMode::Fit);
			out[2] = fitInCell(frames[2].aspect(),
					   {inner.x + bigW + g, inner.y + smallH + g, smallW, smallH}, FitMode::Fit);
		}
		break;
	case Preset::GridTwoByTwo:
		if (frames.size() >= 4) {
			const double cw = (inner.w - g) / 2, ch = (inner.h - g) / 2;
			for (size_t i = 0; i < 4; i++) {
				const double x = inner.x + (i % 2) * (cw + g);
				const double y = inner.y + (i / 2) * (ch + g);
				out[i] = fitInCell(frames[i].aspect(), {x, y, cw, ch}, FitMode::Fit);
			}
		}
		break;
	}
	return out;
}

Rect applyConstraints(const Rect &b, double fromW, double fromH, double toW, double toH, HConstraint h, VConstraint v)
{
	Rect r = b;
	if (!(fromW > 0) || !(fromH > 0) || !(toW > 0) || !(toH > 0))
		return r;
	const double left = b.left(), right = fromW - b.right();
	switch (h) {
	case HConstraint::Left:
		r.x = left;
		break;
	case HConstraint::Right:
		r.x = toW - right - b.w;
		break;
	case HConstraint::Center:
		r.x = toW / 2 + (b.centerX() - fromW / 2) - b.w / 2;
		break;
	case HConstraint::LeftRight:
		r.x = left;
		r.w = std::max(1.0, toW - left - right);
		break;
	case HConstraint::Scale:
		r.x = b.x * toW / fromW;
		r.w = b.w * toW / fromW;
		break;
	case HConstraint::None:
	default:
		break;
	}
	const double top = b.top(), bottom = fromH - b.bottom();
	switch (v) {
	case VConstraint::Top:
		r.y = top;
		break;
	case VConstraint::Bottom:
		r.y = toH - bottom - b.h;
		break;
	case VConstraint::Center:
		r.y = toH / 2 + (b.centerY() - fromH / 2) - b.h / 2;
		break;
	case VConstraint::TopBottom:
		r.y = top;
		r.h = std::max(1.0, toH - top - bottom);
		break;
	case VConstraint::Scale:
		r.y = b.y * toH / fromH;
		r.h = b.h * toH / fromH;
		break;
	case VConstraint::None:
	default:
		break;
	}
	return r;
}

Rect obsRelativeMapping(const Rect &b, double fromW, double fromH, double toW, double toH, bool scaleSize)
{
	if (!(fromH > 0) || !(toH > 0))
		return b;
	const double k = toH / fromH;
	const double w = scaleSize ? b.w * k : b.w;
	const double h = scaleSize ? b.h * k : b.h;
	const double cx = toW / 2 + (b.centerX() - fromW / 2) * k;
	const double cy = toH / 2 + (b.centerY() - fromH / 2) * k;
	return {cx - w / 2, cy - h / 2, w, h};
}

Rect invertObsRelativeMapping(const Rect &a, double fromW, double fromH, double toW, double toH, bool scaleSize)
{
	return obsRelativeMapping(a, toW, toH, fromW, fromH, scaleSize);
}

} // namespace layouts::core
