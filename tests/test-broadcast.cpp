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

bool overlaps(const Rect &a, const Rect &b)
{
	return a.x < b.right() - 1e-6 && b.x < a.right() - 1e-6 && a.y < b.bottom() - 1e-6 && b.y < a.bottom() - 1e-6;
}
} // namespace

TEST("broadcast: every template fills its slots inside the canvas without overlap")
{
	BroadcastSpec spec;
	spec.gap = 8;
	for (const BroadcastTemplate &t : broadcastTemplates()) {
		if (t.landscapeOnly)
			continue; /* inventive layouts overlap on purpose: test-broadcast-inventive.cpp */
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
	/* Portrait: the classic templates only (inventive ones are landscape only). */
	for (int n = 1; n <= 6; n++)
		CHECK(!templatesFor(n, 0, 0, true).empty());
	CHECK(templatesFor(2, 0, 0, true).size() == 2);
	CHECK(templatesFor(4, 1, 0, true).size() == 2); /* column and strip */
	CHECK(templatesFor(0, 1, 0, true).size() == 1);
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

TEST("broadcast: portrait canvas stacks guests and keeps screens 16:9 on top")
{
	const Rect v(0, 0, 1080, 1920);
	BroadcastSpec spec;
	for (const BroadcastTemplate &t : broadcastTemplates()) {
		if (t.landscapeOnly)
			continue;
		const std::vector<Slot> s = buildBroadcast(t.key, v, spec);
		CHECK((int)s.size() == t.cameras + t.screens);
		for (size_t i = 0; i < s.size(); i++) {
			CHECK(inside(v, s[i].cell));
			CHECK(s[i].cell.w > 0 && s[i].cell.h > 0);
			for (size_t j = i + 1; j < s.size(); j++) {
				if (t.key != "Screen1Cam1Pip" && overlaps(s[i].cell, s[j].cell))
					t::fail(__FILE__, __LINE__, t.key + " portrait overlap");
			}
		}
	}
	spec.gap = 0;
	std::vector<Slot> duo = buildBroadcast("Cam2Split", v, spec);
	CHECK(duo[0].cell == Rect(0, 0, 1080, 960));
	CHECK(duo[1].cell == Rect(0, 960, 1080, 960));
	std::vector<Slot> strip = buildBroadcast("Screen1Cam2Strip", v, spec);
	CHECK(strip.back().cell == Rect(0, 0, 1080, 607.5));
	CHECK_NEAR(strip[0].cell.y, 607.5, 1e-9);
	CHECK_NEAR(strip[0].cell.bottom(), 1920, 1e-9);
	CHECK_NEAR(strip[0].cell.w, 540, 1e-9);
	std::vector<Slot> column = buildBroadcast("Screen1Cam2Column", v, spec);
	CHECK_NEAR(column[0].cell.w, 1080, 1e-9); /* one guest per row */
	CHECK_NEAR(column[1].cell.y, column[0].cell.bottom(), 1e-9);
	std::vector<Slot> five = buildBroadcast("Screen1Cam5Strip", v, spec);
	CHECK_NEAR(five[3].cell.w, 540, 1e-9); /* last row of two shares the width */
	/* Square canvas keeps the landscape arrangement. */
	CHECK(buildBroadcast("Cam2Split", Rect(0, 0, 1080, 1080), spec)[1].cell.x > 0);
}
