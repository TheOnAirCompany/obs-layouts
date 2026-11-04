#include "test.hpp"

#include "broadcast.hpp"

using namespace layouts::core;

namespace {
const Rect kCanvas{0, 0, 1920, 1080};

bool inside(const Rect &outer, const Rect &r)
{
	return r.x >= outer.x - 1e-6 && r.y >= outer.y - 1e-6 && r.right() <= outer.right() + 1e-6 &&
	       r.bottom() <= outer.bottom() + 1e-6;
}
} // namespace

TEST("broadcast: every template fills its slots inside the canvas without overlap")
{
	BroadcastSpec spec;
	spec.gap = 8;
	for (const BroadcastTemplate &t : broadcastTemplates()) {
		const auto slots = buildBroadcast(t.key, kCanvas, spec);
		int cams = 0, screens = 0;
		for (const Slot &s : slots) {
			(s.kind == SlotKind::Camera ? cams : screens)++;
			CHECK(inside(kCanvas, s.cell));
			CHECK(s.cell.w > 0 && s.cell.h > 0);
		}
		if (cams != t.cameras || screens != t.screens)
			t::fail(__FILE__, __LINE__, t.key + " slot counts");
		/* No overlap, except the picture-in-picture camera over its screen. */
		if (t.key != "Screen1Cam1Pip") {
			for (size_t i = 0; i < slots.size(); i++)
				for (size_t j = i + 1; j < slots.size(); j++) {
					const Rect &a = slots[i].cell, &b = slots[j].cell;
					const bool overlap = a.x < b.right() - 1e-6 && b.x < a.right() - 1e-6 &&
							     a.y < b.bottom() - 1e-6 && b.y < a.bottom() - 1e-6;
					if (overlap)
						t::fail(__FILE__, __LINE__, t.key + " overlap");
				}
		}
	}
}

TEST("broadcast: templates for guest counts 1 to 6")
{
	for (int n = 1; n <= 6; n++)
		CHECK(!templatesFor(n, 0).empty());
	CHECK(templatesFor(2, 0).size() == 2);
	CHECK(templatesFor(4, 1).size() == 2); /* column and strip */
	CHECK(templatesFor(0, 1).size() == 1);
	CHECK(templatesFor(7, 0).empty());
}

TEST("broadcast: TV duo split and grid geometry")
{
	BroadcastSpec spec;
	spec.gap = 0;
	auto s = buildBroadcast("Cam2Split", kCanvas, spec);
	CHECK(s[0].cell == Rect(0, 0, 960, 1080));
	CHECK(s[1].cell == Rect(960, 0, 960, 1080));
	s = buildBroadcast("Cam4Grid", kCanvas, spec);
	CHECK(s[3].cell == Rect(960, 540, 960, 540));
	s = buildBroadcast("Screen1Cam3Column", kCanvas, spec);
	CHECK(s.back().kind == SlotKind::Screen);
	CHECK_NEAR(s.back().cell.w / s.back().cell.h, 16.0 / 9.0, 1e-9);
	CHECK_NEAR(s[0].cell.y, s.back().cell.y, 1e-9); /* guests aligned with the screen */
}

TEST("broadcast: cover crop fills the cell and keeps faces")
{
	/* 16:9 camera into a 960x1080 half: crop the sides, centered. */
	CoverResult c = coverCell(1920, 1080, 960, 1080);
	CHECK(c.crop.left == c.crop.right);
	CHECK(c.crop.left + c.crop.right == 960);
	CHECK_NEAR(c.scale.x, 1.0, 1e-9);
	/* 4:3 camera into a 16:9 cell: crop top and bottom, more at the bottom. */
	c = coverCell(1440, 1080, 1920, 1080);
	CHECK(c.crop.top + c.crop.bottom == 270);
	CHECK(c.crop.top < c.crop.bottom);
	CHECK_NEAR(c.scale.x, 1920.0 / 1440.0, 1e-9);
	CHECK_NEAR(c.scale.y, 1080.0 / 810.0, 1e-9);
	/* Same aspect: no crop. */
	c = coverCell(1280, 720, 640, 360);
	CHECK(c.crop.left == 0 && c.crop.top == 0);
	CHECK_NEAR(c.scale.x, 0.5, 1e-9);
}
