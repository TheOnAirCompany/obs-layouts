#include "test.hpp"

#include "mirror.hpp"

using namespace layouts::core;

namespace {
const Rect kCanvas{0, 0, 1920, 1080};

bool same(const Rect &a, const Rect &b)
{
	return std::fabs(a.x - b.x) < 1e-6 && std::fabs(a.y - b.y) < 1e-6 && std::fabs(a.w - b.w) < 1e-6 &&
	       std::fabs(a.h - b.h) < 1e-6;
}

std::vector<Slot> sample()
{
	auto slot = [](SlotKind k, const Rect &r) {
		Slot s;
		s.kind = k;
		s.cell = r;
		return s;
	};
	Slot cam = slot(SlotKind::Camera, {0, 0, 640, 1080});
	Slot scr = slot(SlotKind::Screen, {640, 0, 1280, 720});
	scr.rotation = 4;
	Slot chat = slot(SlotKind::Chat, {640, 720, 1280, 360});
	chat.shape = SlotShape::Polygon;
	chat.polygon = {{0.2, 0}, {1, 0}, {1, 1}, {0, 1}};
	return {cam, scr, chat};
}
} // namespace

TEST("mirror: no axis gives the same slots")
{
	const auto in = sample();
	const auto out = mirrorSlots(in, kCanvas, false, false);
	CHECK(out.size() == in.size());
	for (size_t i = 0; i < in.size(); i++) {
		CHECK(same(in[i].cell, out[i].cell));
		CHECK(in[i].rotation == out[i].rotation);
	}
}

TEST("mirror: horizontal swaps sides, keeps order, negates rotation, mirrors polygons")
{
	const auto out = mirrorSlots(sample(), kCanvas, true, false);
	CHECK(out.size() == 3);
	CHECK(out[0].kind == SlotKind::Camera && same(out[0].cell, {1280, 0, 640, 1080}));
	CHECK(out[1].kind == SlotKind::Screen && same(out[1].cell, {0, 0, 1280, 720}));
	CHECK(out[1].rotation == -4);
	CHECK(same(out[2].cell, {0, 720, 1280, 360}));
	/* {0.2,0} becomes {0.8,0}; order reversed to stay clockwise. */
	CHECK(out[2].polygon.size() == 4);
	CHECK(std::fabs(out[2].polygon[3].x - 0.8) < 1e-9 && out[2].polygon[3].y == 0);
	CHECK(out[2].polygon[0].x == 1 && out[2].polygon[0].y == 1);
}

TEST("mirror: vertical moves top to bottom")
{
	const auto out = mirrorSlots(sample(), kCanvas, false, true);
	CHECK(same(out[1].cell, {640, 360, 1280, 720}));
	CHECK(same(out[2].cell, {640, 0, 1280, 360}));
	CHECK(out[1].rotation == -4);
	CHECK(out[2].polygon[0].x == 0 && out[2].polygon[0].y == 0);
}

TEST("mirror: both axes is a half turn (rotation kept) and twice is identity")
{
	const auto in = sample();
	const auto both = mirrorSlots(in, kCanvas, true, true);
	CHECK(both[1].rotation == 4);
	CHECK(same(both[0].cell, {1280, 0, 640, 1080}));
	const Rect offset{100, 50, 1920, 1080};
	for (bool h : {false, true})
		for (bool v : {false, true}) {
			const auto back = mirrorSlots(mirrorSlots(in, offset, h, v), offset, h, v);
			for (size_t i = 0; i < in.size(); i++) {
				CHECK(same(in[i].cell, back[i].cell));
				CHECK(in[i].rotation == back[i].rotation);
				for (size_t k = 0; k < in[i].polygon.size(); k++)
					CHECK(std::fabs(in[i].polygon[k].x - back[i].polygon[k].x) < 1e-9 &&
					      std::fabs(in[i].polygon[k].y - back[i].polygon[k].y) < 1e-9);
			}
		}
}

TEST("mirror: every template stays inside the canvas")
{
	BroadcastSpec spec;
	for (const BroadcastTemplate &t : broadcastTemplates()) {
		const auto out = mirrorSlots(buildBroadcast(t.key, kCanvas, spec), kCanvas, true, true);
		for (const Slot &s : out)
			CHECK(s.cell.x >= -1e-6 && s.cell.y >= -1e-6 && s.cell.x + s.cell.w <= 1920 + 1e-6 &&
			      s.cell.y + s.cell.h <= 1080 + 1e-6);
	}
}
