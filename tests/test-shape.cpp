#include "test.hpp"

#include "shape.hpp"

#include <cmath>

using namespace layouts::core;
using namespace layouts::core::shape;

TEST("shape: auto radius is 4 % of the short side, 6..48 px")
{
	CHECK_NEAR(radiusFor(RadiusMode::Auto, 0, 1920, 1080), 43.2, 1e-9);
	CHECK_NEAR(radiusFor(RadiusMode::Auto, 0, 640, 360), 14.4, 1e-9);
	CHECK_NEAR(radiusFor(RadiusMode::Auto, 0, 100, 100), 6.0, 1e-9);
	CHECK_NEAR(radiusFor(RadiusMode::Auto, 0, 3840, 2160), 48.0, 1e-9);
	CHECK_NEAR(radiusFor(RadiusMode::Auto, 0, 8, 200), 4.0, 1e-9); /* never more than a half side */
	CHECK_NEAR(radiusFor(RadiusMode::Auto, 0, 1080, 1920), 43.2, 1e-9);
}

TEST("shape: pixels, round and none")
{
	CHECK_NEAR(radiusFor(RadiusMode::Pixels, 24, 1920, 1080), 24.0, 1e-9);
	CHECK_NEAR(radiusFor(RadiusMode::Pixels, 900, 1920, 1080), 540.0, 1e-9);
	CHECK_NEAR(radiusFor(RadiusMode::Pixels, -3, 1920, 1080), 0.0, 1e-9);
	CHECK_NEAR(radiusFor(RadiusMode::Round, 0, 400, 400), 200.0, 1e-9);
	CHECK_NEAR(radiusFor(RadiusMode::Round, 0, 600, 120), 60.0, 1e-9);
	CHECK_NEAR(radiusFor(RadiusMode::None, 30, 600, 120), 0.0, 1e-9);
}

TEST("shape: axis scale of a rotated matrix")
{
	const double a = 30.0 * M_PI / 180.0;
	const double sx = 0.5, sy = 2.0;
	/* rows of an OBS matrix: image of the x axis, image of the y axis */
	const Vec2 s = axisScale(sx * std::cos(a), sx * std::sin(a), -sy * std::sin(a), sy * std::cos(a));
	CHECK_NEAR(s.x, 0.5, 1e-12);
	CHECK_NEAR(s.y, 2.0, 1e-12);
	CHECK(!axisAligned(sx * std::cos(a), sx * std::sin(a), -sy * std::sin(a), sy * std::cos(a)));
	CHECK(axisAligned(0.5, 0, 0, 2));
	CHECK(axisAligned(-0.5, 0, 0, 2)); /* flipped, still axis aligned */
}

TEST("shape: convex polygon is a clockwise hull")
{
	/* counter-clockwise input with a point inside and a duplicate */
	const std::vector<Vec2> poly = convexPolygon({{0, 0}, {0, 1}, {1, 1}, {0.5, 0.5}, {1, 0}, {1, 0}});
	CHECK(poly.size() == 4);
	double area = 0;
	for (size_t i = 0; i < poly.size(); i++) {
		const Vec2 a = poly[i], b = poly[(i + 1) % poly.size()];
		area += a.x * b.y - b.x * a.y;
	}
	CHECK(area > 0); /* clockwise on screen, y down */
	CHECK(convexPolygon({{0, 0}, {1, 1}}).empty());
	CHECK(convexPolygon({{0, 0}, {1, 1}, {2, 2}}).empty()); /* collinear */
}

TEST("shape: polygon keeps at most 8 points")
{
	std::vector<Vec2> circle;
	for (int i = 0; i < 20; i++)
		circle.push_back({std::cos(i * M_PI / 10), std::sin(i * M_PI / 10)});
	CHECK(convexPolygon(circle).size() == 8);
}

TEST("shape: signed distance to a diagonal polygon")
{
	/* left part of a diagonal split: (0,0) (0.6,0) (0.4,1) (0,1), in a 100 x 100 cell */
	const std::vector<Vec2> poly = convexPolygon(denormalize({{0, 0}, {0.6, 0}, {0.4, 1}, {0, 1}}, 0, 0, 100, 100));
	CHECK(poly.size() == 4);
	CHECK_NEAR(polygonDistance(poly, {10, 50}), -10.0, 1e-9); /* nearest edge: the left one */
	CHECK(polygonDistance(poly, {49, 50}) < 0);
	CHECK(polygonDistance(poly, {51, 50}) > 0);
	/* on the diagonal: distance 0 */
	CHECK_NEAR(polygonDistance(poly, {50, 50}), 0.0, 1e-9);
	/* 1 px right of the diagonal, measured along its normal */
	const double len = std::hypot(20.0, 100.0);
	CHECK_NEAR(polygonDistance(poly, {51, 50}), 100.0 / len, 1e-9);
	CHECK(polygonDistance({}, {0, 0}) < -1e300);
}
