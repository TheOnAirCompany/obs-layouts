#include "test.hpp"

#include "item-model.hpp"

using namespace layouts::core;

namespace {

ItemModel item(uint32_t w, uint32_t h, Vec2 pos, Vec2 scale = {1, 1}, uint32_t align = kAlignLeft | kAlignTop)
{
	ItemModel m;
	m.sourceWidth = w;
	m.sourceHeight = h;
	m.t.pos = pos;
	m.t.scale = scale;
	m.t.align = align;
	return m;
}

ItemModel with(ItemModel m, const ItemTransform &t)
{
	m.t = t;
	return m;
}

const Affine I = Affine::identity();

} // namespace

TEST("item: unrotated box, top-left alignment")
{
	const ItemModel m = item(1920, 1080, {100, 50}, {0.5, 0.5});
	const VisualFrame f = visualFrame(m);
	CHECK_NEAR(f.quad.p[0].x, 100, 1e-6);
	CHECK_NEAR(f.quad.p[0].y, 50, 1e-6);
	CHECK_NEAR(f.quad.p[2].x, 1060, 1e-6);
	CHECK_NEAR(f.quad.p[2].y, 590, 1e-6);
	CHECK_NEAR(f.width, 960, 1e-6);
	CHECK_NEAR(f.height, 540, 1e-6);
	CHECK_NEAR(f.rotation, 0, 1e-9);
}

TEST("item: center alignment puts pos in the middle")
{
	const ItemModel m = item(1920, 1080, {960, 540}, {0.5, 0.5}, kAlignCenter);
	const Rect b = visualFrame(m).quad.bounds();
	CHECK_NEAR(b.x, 480, 1e-6);
	CHECK_NEAR(b.y, 270, 1e-6);
	CHECK_NEAR(b.w, 960, 1e-6);
}

TEST("item: rotation turns around pos, clockwise")
{
	ItemModel m = item(200, 100, {500, 500});
	m.t.rot = 90;
	const VisualFrame f = visualFrame(m);
	CHECK_NEAR(f.quad.p[0].x, 500, 1e-4);
	CHECK_NEAR(f.quad.p[1].x, 500, 1e-4);
	CHECK_NEAR(f.quad.p[1].y, 700, 1e-4);
	CHECK_NEAR(f.rotation, 90, 1e-6);
	CHECK_NEAR(f.width, 200, 1e-4);
}

TEST("item: crop shrinks the box")
{
	ItemModel m = item(1920, 1080, {0, 0}, {0.5, 0.5});
	m.t.crop.left = 100;
	m.t.crop.right = 20;
	const ItemGeometry g = computeGeometry(m);
	CHECK(g.croppedWidth == 1800);
	CHECK_NEAR(g.boxScale.x, 900, 1e-6);
}

TEST("item: bounds box is the bounds, content scaled inside")
{
	ItemModel m = item(1920, 1080, {0, 0});
	m.t.boundsType = BoundsType::ScaleInner;
	m.t.bounds = {800, 800};
	const ItemGeometry g = computeGeometry(m);
	CHECK_NEAR(g.boxScale.x, 800, 1e-6);
	CHECK_NEAR(g.boxScale.y, 800, 1e-6);
	CHECK_NEAR(g.outputScale.x, 800.0 / 1920.0, 1e-6);
	/* Content is centered vertically in the bounds (bounds alignment center). */
	const Vec2 contentTop = g.draw.map({0, 0});
	CHECK_NEAR(contentTop.y, (800 - 450) / 2.0, 0.6);
}

TEST("item: flip keeps a visual top-left")
{
	ItemModel m = item(100, 50, {0, 0}, {-1, 1});
	const VisualFrame f = visualFrame(m);
	CHECK_NEAR(f.quad.p[0].x, -100, 1e-6);
	CHECK_NEAR(f.quad.p[1].x, 0, 1e-6);
	CHECK_NEAR(f.width, 100, 1e-6);
	CHECK_NEAR(f.rotation, 0, 1e-6);
}

TEST("item: move a reference point to a target")
{
	const ItemModel m = item(400, 200, {10, 10});
	const ItemTransform t = moveRefTo(m, I, RefPoint{1, 1}, {1000, 500});
	const VisualFrame f = visualFrame(with(m, t));
	CHECK_NEAR(f.quad.p[2].x, 1000, 1e-6);
	CHECK_NEAR(f.quad.p[2].y, 500, 1e-6);
}

TEST("item: resize keeps the anchor, keeps flips")
{
	ItemModel m = item(400, 200, {100, 100}, {-1, 1}, kAlignCenter);
	const VisualFrame f0 = visualFrame(m);
	const ItemTransform t = setWorldSize(m, I, 800, 100, RefPoint{0.5, 0.5});
	const VisualFrame f1 = visualFrame(with(m, t));
	CHECK_NEAR(f1.width, 800, 1e-3);
	CHECK_NEAR(f1.height, 100, 1e-3);
	CHECK_NEAR(f1.center().x, f0.center().x, 0.5);
	CHECK_NEAR(f1.center().y, f0.center().y, 0.5);
	CHECK(t.scale.x < 0);
}

TEST("item: resize a rotated item from its top-left")
{
	ItemModel m = item(400, 200, {300, 300});
	m.t.rot = 30;
	const Vec2 tl0 = visualFrame(m).point({0, 0});
	const ItemTransform t = setWorldSize(m, I, 200, 100, RefPoint{0, 0});
	const VisualFrame f1 = visualFrame(with(m, t));
	CHECK_NEAR(f1.point({0, 0}).x, tl0.x, 1e-3);
	CHECK_NEAR(f1.point({0, 0}).y, tl0.y, 1e-3);
	CHECK_NEAR(f1.width, 200, 1e-3);
}

TEST("item: resize with bounds resizes the bounds")
{
	ItemModel m = item(1920, 1080, {0, 0});
	m.t.boundsType = BoundsType::Stretch;
	m.t.bounds = {640, 360};
	const ItemTransform t = setWorldSize(m, I, 1280, 720, RefPoint{0, 0});
	CHECK_NEAR(t.bounds.x, 1280, 1e-6);
	CHECK_NEAR(t.bounds.y, 720, 1e-6);
	CHECK_NEAR(t.scale.x, 1, 0);
}

TEST("item: rotation keeps the center")
{
	const ItemModel m = item(400, 200, {100, 100});
	const Vec2 c0 = visualFrame(m).center();
	const ItemTransform t = setWorldRotation(m, I, 90);
	const VisualFrame f1 = visualFrame(with(m, t));
	CHECK_NEAR(f1.center().x, c0.x, 1e-3);
	CHECK_NEAR(f1.center().y, c0.y, 1e-3);
	CHECK_NEAR(f1.rotation, 90, 1e-4);
	const ItemTransform t2 = rotateBy(with(m, t), I, 180);
	CHECK_NEAR(t2.rot, -90, 1e-9);
}

TEST("item: flip keeps the center")
{
	const ItemModel m = item(400, 200, {100, 100});
	const Vec2 c0 = visualFrame(m).center();
	const ItemTransform t = flip(m, I, Axis::X);
	CHECK(t.scale.x < 0);
	const Vec2 c1 = visualFrame(with(m, t)).center();
	CHECK_NEAR(c1.x, c0.x, 1e-3);
	CHECK_NEAR(c1.y, c0.y, 1e-3);
}

TEST("item: crop keeps the content in place")
{
	const ItemModel m = item(1000, 500, {0, 0});
	Crop c;
	c.left = 100;
	c.top = 40;
	const ItemTransform t = setCrop(m, I, c, RefPoint{0, 0});
	CHECK_NEAR(t.pos.x, 100, 1e-6);
	CHECK_NEAR(t.pos.y, 40, 1e-6);
	CHECK(t.crop.left == 100);

	/* Scaled item: cropping 100 source px moves by 50 canvas px. */
	const ItemModel half = item(1000, 500, {0, 0}, {0.5, 0.5});
	const ItemTransform t2 = setCrop(half, I, c, RefPoint{0, 0});
	CHECK_NEAR(t2.pos.x, 50, 1e-6);
}

TEST("item: crop never removes the whole source")
{
	const ItemModel m = item(100, 100, {0, 0});
	Crop c;
	c.left = 80;
	c.right = 80;
	const ItemTransform t = setCrop(m, I, c, RefPoint{0, 0});
	CHECK(t.crop.left + t.crop.right < 100);
}

TEST("item: fit, fill and stretch into the canvas")
{
	const ItemModel m = item(1000, 1000, {0, 0});
	const Rect canvas{0, 0, 1920, 1080};
	Rect b = visualFrame(with(m, fitInto(m, I, canvas, FitMode::Fit))).quad.bounds();
	CHECK_NEAR(b.x, 420, 1e-3);
	CHECK_NEAR(b.w, 1080, 1e-3);
	b = visualFrame(with(m, fitInto(m, I, canvas, FitMode::Fill))).quad.bounds();
	CHECK_NEAR(b.w, 1920, 1e-3);
	CHECK_NEAR(b.y, -420, 1e-3);
	b = visualFrame(with(m, fitInto(m, I, canvas, FitMode::Stretch))).quad.bounds();
	CHECK_NEAR(b.w, 1920, 1e-3);
	CHECK_NEAR(b.h, 1080, 1e-3);
}

TEST("item: original size and bounds switching keep the anchor")
{
	ItemModel m = item(200, 100, {50, 50}, {2, 2});
	ItemTransform t = originalSize(m, I, RefPoint{0, 0});
	CHECK_NEAR(t.scale.x, 1, 0);
	CHECK_NEAR(t.pos.x, 50, 1e-9);

	t = setBoundsType(m, I, BoundsType::ScaleInner, RefPoint{0.5, 0.5});
	CHECK_NEAR(t.bounds.x, 400, 1e-6);
	CHECK_NEAR(visualFrame(with(m, t)).width, 400, 1e-6);
	const ItemModel b = with(m, t);
	const ItemTransform back = setBoundsType(b, I, BoundsType::None, RefPoint{0.5, 0.5});
	CHECK_NEAR(back.scale.x, 2, 1e-5);
}

TEST("item: group parent converts world deltas to local")
{
	const ItemModel m = item(100, 100, {10, 10});
	const Affine parent = Affine::translation(100, 100) * Affine::scaling(2, 2);
	const ItemTransform t = moveBy(m, parent, {10, 0});
	CHECK_NEAR(t.pos.x, 15, 1e-9);
	const VisualFrame f = visualFrame(m, parent);
	CHECK_NEAR(f.width, 200, 1e-9);
	const ItemTransform s = setWorldSize(m, parent, 100, 100, RefPoint{0, 0});
	CHECK_NEAR(s.scale.x, 0.5, 1e-9);
}

TEST("item: zero-sized sources are left untouched")
{
	const ItemModel m = item(0, 0, {10, 10});
	const ItemTransform t = setWorldSize(m, I, 100, 100, RefPoint{0, 0});
	CHECK(t.pos == m.t.pos);
	CHECK(t.scale == m.t.scale);
}
