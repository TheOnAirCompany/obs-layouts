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

#include "test.hpp"

#include "swap.hpp"

#include <cmath>

using namespace layouts::core;

namespace {

/* A 1920x1080 source placed by coverCell in a cell (like a broadcast camera slot). */
ItemModel camIn(const Rect &cell, uint32_t w = 1920, uint32_t h = 1080)
{
	ItemModel m;
	m.sourceWidth = w;
	m.sourceHeight = h;
	const CoverResult c = coverCell(w, h, cell.w, cell.h);
	m.t.crop = c.crop;
	m.t.scale = c.scale;
	m.t.pos = {cell.x, cell.y};
	return m;
}

void checkSlot(const ItemModel &m, const ItemTransform &t, const SlotRect &s)
{
	ItemModel placed = m;
	placed.t = t;
	const SlotRect got = slotOf(placed);
	CHECK_NEAR(got.center.x, s.center.x, 0.01);
	CHECK_NEAR(got.center.y, s.center.y, 0.01);
	CHECK_NEAR(got.width, s.width, 0.01);
	CHECK_NEAR(got.height, s.height, 0.01);
	CHECK_NEAR(normalizeDegrees(got.rotation - s.rotation), 0, 1e-6);
}

} // namespace

TEST("swap: Duo cameras exchange their cells")
{
	const Rect left{0, 0, 960, 1080}, right{960, 0, 960, 1080};
	const ItemModel a = camIn(left), b = camIn(right);
	const SwapResult r = swapSlots(a, Affine::identity(), FrameMode::Fill, b, Affine::identity(), FrameMode::Fill);
	checkSlot(a, r.a, slotOf(b));
	checkSlot(b, r.b, slotOf(a));
	/* Same cell shape: the crop of each source is the cover crop. */
	CHECK(r.a.crop == coverCell(1920, 1080, 960, 1080).crop);
	CHECK(r.b.crop == coverCell(1920, 1080, 960, 1080).crop);
}

TEST("swap: the crop is recomputed for a cell of another shape (keepTop 0.3)")
{
	const Rect big{0, 0, 1280, 1080}, small{1280, 0, 640, 360};
	const ItemModel a = camIn(big), b = camIn(small);
	const SwapResult r = swapSlots(a, Affine::identity(), FrameMode::Fill, b, Affine::identity(), FrameMode::Fill);
	checkSlot(a, r.a, slotOf(b));
	checkSlot(b, r.b, slotOf(a));
	/* a is now in a 16:9 cell: no crop at all. */
	CHECK(r.a.crop == Crop());
	CHECK_NEAR(std::fabs(r.a.scale.x), 640.0 / 1920.0, 1e-9);
	/* b is in the tall cell: cropped across, centered. */
	CHECK(r.b.crop == coverCell(1920, 1080, 1280, 1080).crop);
	CHECK(r.b.crop.left > 0 && std::abs(r.b.crop.left - r.b.crop.right) <= 1);
}

TEST("swap: a portrait cell crops vertically 30 % from the top")
{
	const Rect wide{0, 0, 1920, 540}, tall{0, 540, 1920, 540};
	ItemModel a = camIn(wide, 1080, 1920); /* portrait phone camera */
	ItemModel b = camIn(tall);
	const SwapResult r = swapSlots(a, Affine::identity(), FrameMode::Fill, b, Affine::identity(), FrameMode::Fill);
	checkSlot(a, r.a, slotOf(b));
	const Crop want = coverCell(1080, 1920, 1920, 540).crop;
	CHECK(std::abs(r.a.crop.top - want.top) <= 1);
	CHECK(std::abs(r.a.crop.bottom - want.bottom) <= 1);
}

TEST("swap: each item keeps its own flips, the rotation stays with the slot")
{
	const Rect left{0, 0, 960, 1080}, right{960, 0, 960, 1080};
	ItemModel a = camIn(left), b = camIn(right);
	a.t = flip(a, Affine::identity(), Axis::X);        /* mirrored webcam */
	b.t = setWorldRotation(b, Affine::identity(), 10); /* tilted slot */
	const SwapResult r = swapSlots(a, Affine::identity(), FrameMode::Fill, b, Affine::identity(), FrameMode::Fill);
	CHECK(r.a.scale.x < 0);
	CHECK(r.a.scale.y > 0);
	CHECK(r.b.scale.x > 0);
	CHECK_NEAR(r.a.rot, 10, 1e-9);
	CHECK_NEAR(r.b.rot, 0, 1e-9);
	checkSlot(a, r.a, slotOf(b));
	checkSlot(b, r.b, slotOf(a));
}

TEST("swap: screens fit inside the cell of the other")
{
	ItemModel a, b;
	a.sourceWidth = 1920;
	a.sourceHeight = 1080;
	a.t.pos = {0, 0};
	a.t.scale = {0.5, 0.5}; /* 960 x 540 */
	b.sourceWidth = 1000;
	b.sourceHeight = 1000;
	b.t.pos = {1000, 0};
	b.t.scale = {0.4, 0.4}; /* 400 x 400 */
	const SwapResult r = swapSlots(a, Affine::identity(), FrameMode::Fit, b, Affine::identity(), FrameMode::Fit);
	CHECK(r.a.crop == Crop());
	CHECK(r.b.crop == Crop());
	CHECK(r.a.boundsType == BoundsType::ScaleInner);
	checkSlot(a, r.a, slotOf(b));
	checkSlot(b, r.b, slotOf(a));
}

TEST("swap: items in a scaled group land on the world cell")
{
	const Rect left{0, 0, 960, 1080}, right{960, 0, 960, 1080};
	const ItemModel a = camIn(left);
	ItemModel b = camIn({480, 0, 480, 540}); /* group scaled x2 */
	const Affine group = Affine::scaling(2, 2);
	CHECK_NEAR(slotOf(b, group).center.x, right.centerX(), 1e-6);
	const SwapResult r = swapSlots(a, Affine::identity(), FrameMode::Fill, b, group, FrameMode::Fill);
	ItemModel pb = b;
	pb.t = r.b;
	const SlotRect got = slotOf(pb, group);
	CHECK_NEAR(got.center.x, left.centerX(), 0.01);
	CHECK_NEAR(got.width, 960, 0.01);
	checkSlot(a, r.a, slotOf(b, group));
}

TEST("swap: compatible roles")
{
	CHECK(canSwap(Role::Camera, Role::Host));
	CHECK(canSwap(Role::Host, Role::Host));
	CHECK(canSwap(Role::Screen, Role::Screen));
	CHECK(!canSwap(Role::Camera, Role::Screen));
	CHECK(!canSwap(Role::Background, Role::Background));
	CHECK(!canSwap(Role::Graphics, Role::Camera));
	CHECK(!canSwap(Role::None, Role::None));
}
