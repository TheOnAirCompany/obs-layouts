#include "test.hpp"

#include "slot-apply.hpp"

using namespace layouts::core;

namespace {

/* A camera placed to cover a cell, like slotTransform does: top-left aligned, cropped. */
ItemModel coverModel(uint32_t w, uint32_t h, const Rect &cell)
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

Slot slot(SlotKind k, const Rect &r, bool host = false)
{
	Slot s;
	s.kind = k;
	s.cell = r;
	s.host = host;
	return s;
}

} // namespace

TEST("slot-apply: a rotated cover keeps its size and center, turned by the slot angle")
{
	const Rect cell{400, 200, 600, 500};
	for (double deg : {-12.0, 5.0, 30.0, 90.0, 180.0}) {
		ItemModel m = coverModel(1920, 1080, cell);
		m.t = rotateAround(m.t, cell.center(), deg);
		const VisualFrame f = visualFrame(m);
		CHECK_NEAR(f.width, cell.w, 0.5);
		CHECK_NEAR(f.height, cell.h, 0.5);
		CHECK_NEAR(f.center().x, cell.centerX(), 1e-3);
		CHECK_NEAR(f.center().y, cell.centerY(), 1e-3);
		const double diff = std::fmod(f.rotation - deg + 720.0, 360.0);
		CHECK(diff < 1e-6 || diff > 360.0 - 1e-6);
	}
}

TEST("slot-apply: a flipped item stays flipped and centered when rotated")
{
	const Rect cell{100, 100, 400, 300};
	ItemModel m = coverModel(1280, 720, cell);
	m.t = flip(m, Affine::identity(), Axis::X);
	const VisualFrame before = visualFrame(m);
	m.t = rotateAround(m.t, cell.center(), 15);
	const VisualFrame f = visualFrame(m);
	CHECK(m.t.scale.x < 0);
	CHECK_NEAR(f.center().x, before.center().x, 1e-3);
	CHECK_NEAR(f.center().y, before.center().y, 1e-3);
	CHECK_NEAR(f.width, cell.w, 0.5);
	CHECK_NEAR(f.height, cell.h, 0.5);
}

TEST("slot-apply: no rotation changes nothing")
{
	ItemTransform t;
	t.pos = {10, 20};
	const ItemTransform r = rotateAround(t, {500, 500}, 0);
	CHECK(r.pos == t.pos);
	CHECK(r.rot == 0);
}

TEST("slot-apply: host slots take a host first, wherever they are")
{
	std::vector<Slot> slots{slot(SlotKind::Camera, {0, 0, 10, 10}), slot(SlotKind::Camera, {10, 0, 10, 10}, true),
				slot(SlotKind::Screen, {20, 0, 10, 10})};
	/* Items: a guest camera, a screen, the host. */
	const std::vector<SlotKind> kinds{SlotKind::Camera, SlotKind::Screen, SlotKind::Camera};
	const std::vector<bool> hosts{false, false, true};
	const std::vector<int> a = assignItems(slots, kinds, hosts);
	CHECK(a.size() == 3);
	CHECK(a[0] == 0);
	CHECK(a[1] == 2);
	CHECK(a[2] == 1);
}

TEST("slot-apply: host slot without a host takes any camera, extra slots stay empty")
{
	std::vector<Slot> slots{slot(SlotKind::Camera, {0, 0, 10, 10}, true), slot(SlotKind::Camera, {10, 0, 10, 10}),
				slot(SlotKind::Chat, {20, 0, 10, 10})};
	const std::vector<int> a = assignItems(slots, {SlotKind::Camera}, {false});
	CHECK(a[0] == 0);
	CHECK(a[1] == -1);
	CHECK(a[2] == -1);
}

TEST("slot-apply: slots stack in template order, an inset goes in front of its host cell")
{
	std::vector<Slot> slots{slot(SlotKind::Camera, {1500, 800, 300, 200}),
				slot(SlotKind::Screen, {0, 0, 1920, 1080}),
				slot(SlotKind::Camera, {100, 100, 400, 400}),
				slot(SlotKind::Camera, {300, 300, 400, 400})};
	const std::vector<size_t> o = stackOrder(slots);
	CHECK(o.size() == 4);
	/* The screen first (back), then the inset, then the overlapping pair in template order. */
	CHECK(o[0] == 1);
	CHECK(o[1] == 0);
	CHECK(o[2] == 2);
	CHECK(o[3] == 3);
}

TEST("slot-apply: Pip template draws its camera over the screen")
{
	const auto slots = buildBroadcast("Screen1Cam1Pip", {0, 0, 1920, 1080}, BroadcastSpec{});
	const std::vector<size_t> o = stackOrder(slots);
	CHECK(o.size() == 2);
	CHECK(slots[o.back()].kind == SlotKind::Camera);
}

TEST("slot-apply: slot style for round, polygon and frame")
{
	Style s;
	Slot plain = slot(SlotKind::Camera, {0, 0, 10, 10});
	CHECK(!applySlotStyle(plain, s));

	Slot round = plain;
	round.shape = SlotShape::Round;
	CHECK(applySlotStyle(round, s));
	CHECK(s.radiusMode == RadiusMode::Round);

	Style p;
	Slot poly = plain;
	poly.shape = SlotShape::Polygon;
	poly.polygon = {{0, 0}, {1, 0}, {0.8, 1}, {0, 1}};
	poly.frame = true;
	CHECK(applySlotStyle(poly, p));
	CHECK(p.polygon.size() == 4);
	CHECK(p.border);

	CHECK(clearSlotStyle(p));
	CHECK(p.polygon.empty());
	CHECK(!p.border);
	CHECK(p.radiusMode == RadiusMode::Auto);

	Style custom;
	custom.shadow = ShadowPreset::Soft;
	applySlotStyle(round, custom);
	CHECK(!clearSlotStyle(custom)); /* the user's shadow stays */
	CHECK(custom.shadow == ShadowPreset::Soft);
	CHECK(custom.radiusMode == RadiusMode::Auto);
}

TEST("style: radius modes")
{
	Style s;
	CHECK_NEAR(styleRadius(s, 1920, 1080), 43.2, 1e-9);
	CHECK_NEAR(styleRadius(s, 100, 50), 6, 1e-9);
	CHECK_NEAR(styleRadius(s, 4000, 3000), 48, 1e-9);
	s.radiusMode = RadiusMode::Pixels;
	s.radiusPx = 20;
	CHECK_NEAR(styleRadius(s, 400, 300), 20, 1e-9);
	CHECK_NEAR(styleRadius(s, 30, 30), 15, 1e-9);
	s.radiusMode = RadiusMode::Round;
	CHECK_NEAR(styleRadius(s, 400, 300), 150, 1e-9);
	s.radiusMode = RadiusMode::None;
	CHECK_NEAR(styleRadius(s, 400, 300), 0, 1e-9);
}

TEST("style: JSON round trip")
{
	Style s;
	s.radiusMode = RadiusMode::Pixels;
	s.radiusPx = 12.5;
	s.border = true;
	s.borderWidth = 6;
	s.borderColor = 0xFF112233;
	s.shadow = ShadowPreset::Custom;
	s.shadowBlur = 10;
	s.shadowOffsetX = -3;
	s.shadowOffsetY = 4;
	s.shadowOpacity = 0.5;
	s.shadowColor = 0x80000000;
	s.fadeShadowInTransition = false;
	s.polygon = {{0, 0}, {1, 0}, {0.75, 1}, {0, 1}};
	Style r;
	CHECK(fromJson(toJson(s), r));
	CHECK(r.radiusMode == RadiusMode::Pixels);
	CHECK(r.radiusPx == 12.5);
	CHECK(r.border);
	CHECK(r.borderWidth == 6);
	CHECK(r.borderColor == 0xFF112233);
	CHECK(r.shadow == ShadowPreset::Custom);
	CHECK(r.shadowBlur == 10);
	CHECK(r.shadowOffsetX == -3);
	CHECK(r.shadowOffsetY == 4);
	CHECK(r.shadowOpacity == 0.5);
	CHECK(r.shadowColor == 0x80000000);
	CHECK(!r.fadeShadowInTransition);
	CHECK(r.polygon.size() == 4);
	CHECK(r.polygon[2] == Vec2(0.75, 1));
}

TEST("style: JSON defaults, unknown keys and malformed input")
{
	Style r;
	CHECK(fromJson("{ \"radiusMode\" : \"round\", \"future\": {\"a\": [1, null, \"x\\\"y\"]} }", r));
	CHECK(r.radiusMode == RadiusMode::Round);
	CHECK(!r.border);
	CHECK(r.borderWidth == 4);
	CHECK(r.polygon.empty());

	Style kept;
	kept.border = true;
	CHECK(!fromJson("{\"border\": false", kept));
	CHECK(!fromJson("[1, 2]", kept));
	CHECK(!fromJson("", kept));
	CHECK(!fromJson("{\"a\":1} trailing", kept));
	CHECK(kept.border);
}
