#include "test.hpp"

#include "morph.hpp"

using namespace layouts::core;

namespace {

ItemModel item(double x, double y, double sx, double sy, uint32_t sw = 1920, uint32_t sh = 1080)
{
	ItemModel m;
	m.sourceWidth = sw;
	m.sourceHeight = sh;
	m.t.pos = {x, y};
	m.t.scale = {sx, sy};
	return m;
}

/* The visible content quad (what the viewer sees), clip-to-bounds ignored. */
Quad content(const ItemModel &m)
{
	const ItemGeometry g = computeGeometry(m);
	const double w = g.croppedWidth, h = g.croppedHeight;
	return {{g.draw.map({0, 0}), g.draw.map({w, 0}), g.draw.map({w, h}), g.draw.map({0, h})}};
}

void checkSameQuad(const Quad &a, const Quad &b, double eps)
{
	for (int i = 0; i < 4; i++) {
		CHECK_NEAR(a.p[i].x, b.p[i].x, eps);
		CHECK_NEAR(a.p[i].y, b.p[i].y, eps);
	}
}

} // namespace

TEST("morph: ease in-out cubic")
{
	CHECK_NEAR(easeInOutCubic(0), 0, 1e-12);
	CHECK_NEAR(easeInOutCubic(0.5), 0.5, 1e-12);
	CHECK_NEAR(easeInOutCubic(1), 1, 1e-12);
	CHECK(easeInOutCubic(0.25) < 0.25);
	CHECK(easeInOutCubic(0.75) > 0.75);
	CHECK_NEAR(easeInOutCubic(-1), 0, 1e-12);
	CHECK_NEAR(easeInOutCubic(2), 1, 1e-12);
}

TEST("morph: canonical transform shows the same picture")
{
	ItemModel m = item(100, 50, 0.5, 0.5);
	m.t.crop = {100, 20, 40, 60};
	m.t.rot = 30;
	ItemModel c = m;
	c.t = transformFor(visualState(m));
	checkSameQuad(content(m), content(c), 1e-3);
	CHECK(c.t.crop == m.t.crop);
}

TEST("morph: canonical transform keeps flips")
{
	ItemModel m = item(400, 300, -0.5, 0.25);
	m.t.rot = -45;
	m.t.crop = {10, 0, 0, 30};
	const VisualState v = visualState(m);
	CHECK(v.flipX);
	CHECK(!v.flipY);
	ItemModel c = m;
	c.t = transformFor(v);
	checkSameQuad(content(m), content(c), 1e-3);
}

TEST("morph: bounds fit is turned into the letterboxed content")
{
	ItemModel m = item(0, 0, 1, 1);
	m.t.boundsType = BoundsType::ScaleInner;
	m.t.bounds = {960, 960};
	const VisualState v = visualState(m);
	CHECK_NEAR(v.width, 960, 1e-3);
	CHECK_NEAR(v.height, 540, 1e-3);
	CHECK_NEAR(v.center.x, 480, 1e-3);
	CHECK_NEAR(v.center.y, 480, 1e-3);
	ItemModel c = m;
	c.t = transformFor(v);
	checkSameQuad(content(m), content(c), 1e-3);
}

TEST("morph: clip to bounds becomes a crop window")
{
	ItemModel m = item(0, 0, 1, 1);
	m.t.boundsType = BoundsType::ScaleOuter;
	m.t.bounds = {540, 540};
	m.t.cropToBounds = true;
	const VisualState v = visualState(m);
	CHECK_NEAR(v.width, 540, 1e-3);
	CHECK_NEAR(v.height, 540, 1e-3);
	CHECK_NEAR(v.center.x, 270, 1e-3);
	CHECK_NEAR(v.center.y, 270, 1e-3);
}

TEST("morph: ends are exact, middle is between")
{
	const ItemModel a = item(0, 0, 0.5, 0.5);
	ItemModel b = item(960, 540, 0.25, 0.25);
	b.t.crop = {200, 0, 200, 0};
	const ItemTransform t0 = interpolate(a, b, 0.0);
	const ItemTransform t1 = interpolate(a, b, 1.0);
	CHECK(t0.pos == a.t.pos);
	CHECK(t1.pos == b.t.pos && t1.crop == b.t.crop);

	ItemModel mid = b;
	mid.t = interpolate(a, b, 0.5);
	const VisualState va = visualState(a), vb = visualState(b), vm = visualState(mid);
	CHECK_NEAR(vm.center.x, (va.center.x + vb.center.x) / 2, 1e-3);
	CHECK_NEAR(vm.width, (va.width + vb.width) / 2, 1e-3);
	CHECK_NEAR(vm.height, (va.height + vb.height) / 2, 1e-3);
	CHECK(mid.t.crop == (Crop{100, 0, 100, 0}));
}

TEST("morph: visible rect moves smoothly while the crop changes")
{
	/* Same visible rect at both ends, only the crop window changes: the rect must not move. */
	ItemModel a = item(100, 100, 0.5, 0.5);
	ItemModel b = a;
	b.t.crop = {480, 0, 0, 0};
	b.t.scale = {0.5 * 1920.0 / 1440.0, 0.5};
	for (double t = 0.1; t < 1.0; t += 0.1) {
		ItemModel m = a;
		m.t = interpolate(a, b, t);
		const VisualState v = visualState(m);
		CHECK_NEAR(v.width, 960, 1e-3);
		CHECK_NEAR(v.height, 540, 1e-3);
		CHECK_NEAR(v.center.x, 580, 1e-3);
	}
}

TEST("morph: rotation takes the shortest way")
{
	ItemModel a = item(0, 0, 1, 1), b = a;
	a.t.rot = 170;
	b.t.rot = -170;
	ItemModel m = a;
	m.t = interpolate(a, b, 0.5);
	CHECK_NEAR(normalizeDegrees(m.t.rot), 180, 1e-3);
}

TEST("morph: a changing flip goes through zero width")
{
	const ItemModel a = item(0, 0, 0.5, 0.5);
	ItemModel b = a;
	b.t.scale.x = -0.5;
	b.t.pos.x = 960; /* same box once flipped (top-left alignment) */
	ItemModel m = a;
	m.t = interpolate(a, b, 0.25);
	CHECK(m.t.scale.x > 0);
	m.t = interpolate(a, b, 0.75);
	CHECK(m.t.scale.x < 0);
	m.t = interpolate(a, b, 0.5);
	CHECK(std::fabs(m.t.scale.x) < 1e-3);
}

TEST("morph: no source size falls back to a plain lerp")
{
	const ItemModel a = item(0, 0, 1, 1, 0, 0);
	const ItemModel b = item(100, 50, 2, 2, 0, 0);
	const ItemTransform t = interpolate(a, b, 0.5);
	CHECK_NEAR(t.pos.x, 50, 1e-9);
	CHECK_NEAR(t.scale.y, 1.5, 1e-9);
}

TEST("morph: plan sorts moves, entries and exits")
{
	std::vector<MorphItemState> cur = {
		{1, item(0, 0, 0.5, 0.5), true, 1.0},
		{2, item(960, 0, 0.5, 0.5), true, 1.0},
		{3, item(0, 540, 0.5, 0.5), false, 1.0},
		{4, item(960, 540, 0.5, 0.5), true, 1.0},
	};
	std::vector<MorphItemState> targets = {
		{1, item(0, 0, 1, 1), true, 1.0},          /* grows */
		{2, item(960, 0, 0.5, 0.5), true, 1.0},    /* unchanged */
		{3, item(0, 540, 0.5, 0.5), true, 1.0},    /* enters */
		{4, item(960, 540, 0.5, 0.5), false, 1.0}, /* leaves */
		{5, item(10, 10, 0.1, 0.1), true, 1.0},    /* not in the scene list: enters */
	};
	const MorphPlan p = planMorph(cur, targets);
	CHECK(p.tracks.size() == 4);
	CHECK(p.tracks[0].id == 1 && p.tracks[0].kind == TrackKind::Move);
	CHECK(p.tracks[1].id == 3 && p.tracks[1].kind == TrackKind::Enter);
	CHECK(p.tracks[2].id == 4 && p.tracks[2].kind == TrackKind::Leave);
	CHECK(p.tracks[3].id == 5 && p.tracks[3].kind == TrackKind::Enter);

	const auto half = sampleMorph(p, 0.5);
	CHECK_NEAR(half[1].opacity, 0.5, 1e-9);
	CHECK_NEAR(half[2].opacity, 0.5, 1e-9);
	CHECK(half[1].t.pos == targets[2].model.t.pos); /* enters at its place */
	CHECK(half[2].t.pos == cur[3].model.t.pos);     /* leaves from its place */

	const auto end = sampleMorph(p, 1.0);
	CHECK(end[0].t.scale == targets[0].model.t.scale);
	CHECK_NEAR(end[0].opacity, 1, 1e-12);
	CHECK_NEAR(end[2].opacity, 0, 1e-12);
}

TEST("morph: an interrupted fade resumes from its level")
{
	std::vector<MorphItemState> cur = {{1, item(0, 0, 1, 1), true, 0.4}};
	std::vector<MorphItemState> targets = {{1, item(0, 0, 1, 1), true, 1.0}};
	const MorphPlan p = planMorph(cur, targets);
	CHECK(p.tracks.size() == 1);
	CHECK_NEAR(sampleMorph(p, 0.0)[0].opacity, 0.4, 1e-12);
}
