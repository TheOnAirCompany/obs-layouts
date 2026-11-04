#include "test.hpp"

#include "geometry.hpp"
#include "grid.hpp"

using namespace layouts::core;

TEST("geometry: affine compose and invert")
{
	const Affine m = Affine::translation(10, 20) * Affine::rotationDeg(90) * Affine::scaling(2, 3);
	const Vec2 p = m.map({1, 0});
	CHECK_NEAR(p.x, 10, 1e-9);
	CHECK_NEAR(p.y, 22, 1e-9);
	const Vec2 back = m.inverted().map(p);
	CHECK_NEAR(back.x, 1, 1e-9);
	CHECK_NEAR(back.y, 0, 1e-9);
	CHECK_NEAR(m.rotationDeg(), 90, 1e-9);
}

TEST("geometry: rotation is clockwise on screen (y down)")
{
	const Vec2 p = Affine::rotationDeg(90).map({1, 0});
	CHECK_NEAR(p.x, 0, 1e-12);
	CHECK_NEAR(p.y, 1, 1e-12);
}

TEST("geometry: normalize degrees")
{
	CHECK_NEAR(normalizeDegrees(270), -90, 1e-12);
	CHECK_NEAR(normalizeDegrees(-180), 180, 1e-12);
	CHECK_NEAR(normalizeDegrees(540), 180, 1e-12);
	CHECK_NEAR(normalizeDegrees(-30), -30, 1e-12);
}

TEST("geometry: rect union, intersection, containment")
{
	const Rect a{0, 0, 10, 10}, b{20, 5, 10, 10};
	const Rect u = a.united(b);
	CHECK(u == Rect(0, 0, 30, 15));
	CHECK(!a.intersects(b));
	CHECK(u.contains(a));
	CHECK(u.contains(Vec2{30, 15}));
}

TEST("geometry: quad hit testing for a rotated box")
{
	const Affine m = Affine::rotationDeg(45);
	const Quad q{{m.map({0, 0}), m.map({10, 0}), m.map({10, 10}), m.map({0, 10})}};
	CHECK(q.contains(m.map({5, 5})));
	CHECK(!q.contains({9, 0.5}));
	CHECK(q.intersects(Rect{-1, 5, 2, 2}));
	CHECK(!q.intersects(Rect{8, -3, 2, 2}));
}

TEST("geometry: reference points")
{
	{
		const RefPoint c{0.5, 0.5};
		CHECK(RefPoint::fromIndex(4) == c);
	}
	{
		const RefPoint c{1, 1};
		CHECK(RefPoint::fromIndex(8) == c);
	}
	{
		const RefPoint c{1, 0};
		CHECK(c.toIndex() == 2);
	}
	const Vec2 p = pointInRect(Rect{10, 20, 100, 50}, RefPoint{1, 0.5});
	CHECK_NEAR(p.x, 110, 1e-12);
	CHECK_NEAR(p.y, 45, 1e-12);
}

TEST("grid: ruler ticks follow the zoom")
{
	CHECK_NEAR(chooseRulerTicks(1.0).major, 100, 0);
	CHECK_NEAR(chooseRulerTicks(0.5).major, 200, 0);
	CHECK_NEAR(chooseRulerTicks(0.25).major, 500, 0);
	CHECK_NEAR(chooseRulerTicks(4.0).major, 20, 0);
	CHECK_NEAR(chooseRulerTicks(64.0).major, 1, 0);
	CHECK(chooseRulerTicks(64.0).subdivisions == 1);
	CHECK(chooseRulerTicks(1.0).subdivisions == 10);
}

TEST("grid: square lines and nearest line")
{
	SquareGrid g;
	g.size = 16;
	g.offsetX = 4;
	const auto lines = squareLines(g, Axis::X, 0, 40);
	CHECK(lines.size() == 3);
	CHECK_NEAR(lines[0], 4, 0);
	CHECK_NEAR(lines[2], 36, 0);
	CHECK_NEAR(nearestSquareLine(g, Axis::X, 27), 20, 0);
	CHECK_NEAR(nearestSquareLine(g, Axis::Y, 25), 32, 0);
}

TEST("grid: column tracks, stretch and fixed")
{
	TrackGrid g;
	g.count = 12;
	g.gutter = 20;
	g.margin = 40;
	auto t = computeTracks(g, 1920);
	CHECK(t.size() == 12);
	CHECK_NEAR(t[0].start, 40, 1e-9);
	CHECK_NEAR(t[11].end, 1880, 1e-9);
	CHECK_NEAR(t[0].end - t[0].start, (1920 - 80 - 220) / 12.0, 1e-9);

	g.align = TrackAlign::Center;
	g.count = 2;
	g.trackSize = 100;
	t = computeTracks(g, 1000);
	CHECK_NEAR(t[0].start, 390, 1e-9);
	CHECK_NEAR(t[1].end, 610, 1e-9);

	g.count = 0;
	CHECK(computeTracks(g, 1000).empty());
}
