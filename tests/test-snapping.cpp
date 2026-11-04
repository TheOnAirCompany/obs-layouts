#include "test.hpp"

#include "snapping.hpp"

using namespace layouts::core;

namespace {

SnapContext ctx()
{
	SnapContext c;
	c.canvas = {0, 0, 1920, 1080};
	return c;
}

SnapSettings all()
{
	SnapSettings s;
	s.grid = true;
	return s;
}

} // namespace

TEST("snap: canvas edge within threshold")
{
	const SnapResult r = snapMove(Rect{4, 300, 100, 100}, ctx(), all(), 6);
	CHECK(r.snappedX);
	CHECK_NEAR(r.delta.x, -4, 1e-9);
	CHECK(!r.snappedY);
	CHECK(!r.lines.empty());
}

TEST("snap: nothing beyond the threshold")
{
	const SnapResult r = snapMove(Rect{20, 300, 100, 100}, ctx(), all(), 6);
	CHECK(!r.snappedX);
	CHECK(r.delta == Vec2(0, 0));
}

TEST("snap: canvas center")
{
	const SnapResult r = snapMove(Rect{908, 0 + 500, 100, 20}, ctx(), all(), 6);
	CHECK(r.snappedX);
	CHECK_NEAR(r.delta.x, 2, 1e-9);
}

TEST("snap: a guide wins over a closer item edge")
{
	SnapContext c = ctx();
	c.guideX = {505};
	c.others = {Rect{501, 600, 50, 50}};
	const SnapResult r = snapMove(Rect{500, 100, 100, 100}, c, all(), 6);
	CHECK(r.snappedX);
	CHECK_NEAR(r.delta.x, 5, 1e-9);
	CHECK(r.lines.front().kind == SnapKind::Guide);
}

TEST("snap: grid wins over items, items over canvas")
{
	SnapContext c = ctx();
	SquareGrid g;
	g.size = 50;
	c.square = g;
	c.others = {Rect{1203, 0, 10, 10}};
	SnapResult r = snapMove(Rect{1204, 500, 10, 10}, c, all(), 6);
	CHECK_NEAR(r.delta.x, -4, 1e-9); /* grid line 1200 */

	SnapSettings noGrid = all();
	noGrid.grid = false;
	r = snapMove(Rect{1204, 500, 10, 10}, c, noGrid, 6);
	CHECK_NEAR(r.delta.x, -1, 1e-9); /* item edge 1203 */
}

TEST("snap: disabled settings never move")
{
	SnapSettings s = all();
	s.enabled = false;
	const SnapResult r = snapMove(Rect{2, 2, 10, 10}, ctx(), s, 6);
	CHECK(!r.snappedX);
	CHECK(!r.snappedY);
}

TEST("snap: centered between two neighbours (equal spacing)")
{
	SnapContext c = ctx();
	c.others = {Rect{100, 500, 100, 100}, Rect{700, 500, 100, 100}};
	SnapSettings s = all();
	s.canvas = false;
	/* room 500, size 100: centered start = 400; start at 397 */
	const SnapResult r = snapMove(Rect{397, 520, 100, 50}, c, s, 6);
	CHECK(r.snappedX);
	CHECK_NEAR(r.delta.x, 3, 1e-9);
	CHECK(!r.spacings.empty());
	CHECK_NEAR(r.spacings[0].gap, 200, 1e-9);
}

TEST("snap: same gap as existing neighbours")
{
	SnapContext c = ctx();
	c.others = {Rect{0, 0, 100, 100}, Rect{140, 0, 100, 100}};
	SnapSettings s = all();
	s.canvas = false;
	/* gap 40: next start at 280; moving starts at 283 */
	const SnapResult r = snapMove(Rect{283, 10, 50, 50}, c, s, 6);
	CHECK(r.snappedX);
	CHECK_NEAR(r.delta.x, -3, 1e-9);
	CHECK(!r.spacings.empty());
}

TEST("snap: single value (resize edge, guide drag)")
{
	SnapContext c = ctx();
	c.guideY = {300};
	const ValueSnap v = snapValue(303, Axis::Y, c, all(), 6, 0, 100);
	CHECK(v.snapped);
	CHECK_NEAR(v.value, 300, 0);
	CHECK(v.line && v.line->kind == SnapKind::Guide);
}

TEST("measure: gap between separated rectangles")
{
	const auto m = measureBetween(Rect{0, 0, 100, 100}, Rect{124, 20, 100, 100});
	CHECK(m.size() == 1);
	CHECK(m[0].axis == Axis::X);
	CHECK_NEAR(m[0].value(), 24, 1e-9);
	CHECK_NEAR(m[0].cross, 60, 1e-9);
}

TEST("measure: distances to a containing rectangle (canvas)")
{
	const auto m = measureBetween(Rect{100, 50, 200, 100}, Rect{0, 0, 1920, 1080});
	CHECK(m.size() == 4);
	double sum = 0;
	for (const Measure &x : m)
		sum += x.value();
	CHECK_NEAR(sum, 100 + (1920 - 300) + 50 + (1080 - 150), 1e-9);
}
