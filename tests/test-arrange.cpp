#include "test.hpp"

#include "arrange.hpp"

using namespace layouts::core;

namespace {

std::vector<ArrangeItem> three()
{
	return {{Rect{0, 0, 100, 50}, false}, {Rect{300, 100, 50, 50}, false}, {Rect{150, 40, 20, 80}, false}};
}

Rect moved(const ArrangeItem &i, const Vec2 &d)
{
	return i.box.translated(d);
}

} // namespace

TEST("arrange: align left / right / centers to the selection")
{
	const auto items = three();
	const Rect ref = selectionBounds(items);
	CHECK(ref == Rect(0, 0, 350, 150));

	auto d = align(items, AlignEdge::Left, ref);
	for (size_t i = 0; i < items.size(); i++)
		CHECK_NEAR(moved(items[i], d[i]).left(), 0, 1e-9);

	d = align(items, AlignEdge::Right, ref);
	for (size_t i = 0; i < items.size(); i++)
		CHECK_NEAR(moved(items[i], d[i]).right(), 350, 1e-9);

	d = align(items, AlignEdge::VCenter, ref);
	for (size_t i = 0; i < items.size(); i++) {
		CHECK_NEAR(moved(items[i], d[i]).centerY(), 75, 1e-9);
		CHECK_NEAR(d[i].x, 0, 0);
	}
}

TEST("arrange: align to canvas and to a key object")
{
	const auto items = three();
	const Rect canvas{0, 0, 1920, 1080};
	auto ref = alignReference(items, AlignTarget::Canvas, canvas, -1);
	auto d = align(items, AlignEdge::Bottom, *ref);
	CHECK_NEAR(moved(items[1], d[1]).bottom(), 1080, 1e-9);

	ref = alignReference(items, AlignTarget::KeyObject, canvas, 1);
	d = align(items, AlignEdge::Top, *ref, 1);
	CHECK_NEAR(moved(items[0], d[0]).top(), 100, 1e-9);
	CHECK(d[1] == Vec2(0, 0));

	CHECK(!alignReference(items, AlignTarget::KeyObject, canvas, 7));
	CHECK(!alignReference({}, AlignTarget::Selection, canvas, -1));
}

TEST("arrange: locked items never move but count in the bounds")
{
	auto items = three();
	items[1].locked = true;
	const Rect ref = selectionBounds(items);
	const auto d = align(items, AlignEdge::Right, ref);
	CHECK(d[1] == Vec2(0, 0));
	CHECK_NEAR(moved(items[0], d[0]).right(), 350, 1e-9);
	CHECK(movableCount(items) == 2);
}

TEST("arrange: distribute centers")
{
	std::vector<ArrangeItem> items = {{Rect{0, 0, 10, 10}},
					  {Rect{20, 0, 10, 10}},
					  {Rect{100, 0, 30, 10}},
					  {Rect{200, 0, 10, 10}}};
	const auto d = distributeCenters(items, Axis::X);
	/* centers: 5 .. 205, step 200/3 */
	CHECK_NEAR(moved(items[1], d[1]).centerX(), 5 + 200.0 / 3.0, 1e-9);
	CHECK_NEAR(moved(items[2], d[2]).centerX(), 5 + 400.0 / 3.0, 1e-9);
	CHECK(d[0] == Vec2(0, 0));
	CHECK(d[3] == Vec2(0, 0));
}

TEST("arrange: distribute spacing makes equal gaps")
{
	std::vector<ArrangeItem> items = {{Rect{0, 0, 100, 10}}, {Rect{110, 0, 50, 10}}, {Rect{400, 0, 100, 10}}};
	const auto d = distributeSpacing(items, Axis::X);
	const Rect b = moved(items[1], d[1]);
	const double g1 = b.left() - 100;
	const double g2 = 400 - b.right();
	CHECK_NEAR(g1, g2, 1e-9);
	CHECK_NEAR(g1, 125, 1e-9);
	auto gap = uniformGap({{Rect{0, 0, 100, 10}}, {b}, {Rect{400, 0, 100, 10}}}, Axis::X);
	CHECK(gap.has_value());
	CHECK_NEAR(*gap, 125, 1e-9);
}

TEST("arrange: distribution needs three movable items")
{
	std::vector<ArrangeItem> items = {{Rect{0, 0, 10, 10}}, {Rect{50, 0, 10, 10}}};
	const auto d = distributeSpacing(items, Axis::X);
	CHECK(d[0] == Vec2(0, 0));
	CHECK(d[1] == Vec2(0, 0));
}

TEST("arrange: exact spacing respects spatial order")
{
	std::vector<ArrangeItem> items = {{Rect{500, 0, 100, 50}}, {Rect{0, 300, 100, 50}}, {Rect{200, 10, 40, 50}}};
	const auto d = spaceExactly(items, Axis::X, 24);
	CHECK(d[1] == Vec2(0, 0)); /* leftmost stays */
	CHECK_NEAR(moved(items[2], d[2]).left(), 124, 1e-9);
	CHECK_NEAR(moved(items[0], d[0]).left(), 124 + 40 + 24, 1e-9);
	CHECK_NEAR(d[0].y, 0, 0);

	const auto v = spaceExactly(items, Axis::Y, 0);
	CHECK_NEAR(moved(items[2], v[2]).top(), 50, 1e-9);
}

TEST("arrange: uniform gap detects unequal gaps")
{
	std::vector<ArrangeItem> items = {{Rect{0, 0, 10, 10}}, {Rect{20, 0, 10, 10}}, {Rect{50, 0, 10, 10}}};
	CHECK(!uniformGap(items, Axis::X).has_value());
}
