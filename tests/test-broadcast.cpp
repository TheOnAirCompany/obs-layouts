#include "test.hpp"

#include "broadcast.hpp"

#include <algorithm>

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
		const auto slots = buildBroadcast(t.key, kCanvas, spec);
		int cams = 0, screens = 0;
		for (const Slot &s : slots) {
			(s.kind == SlotKind::Camera ? cams : screens)++;
			CHECK(inside(kCanvas, s.cell));
			CHECK(s.cell.w > 0 && s.cell.h > 0);
		}
		if (cams != t.cameras || screens != t.screens)
			t::fail(__FILE__, __LINE__, t.key + " slot counts");
		/* No overlap, except the picture-in-picture insets. */
		if (t.key.find("Pip") == std::string::npos) {
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
	int duos = 0;
	for (const BroadcastTemplate &t : templatesFor(2, 0))
		duos += t.landscapeOnly ? 0 : 1;
	CHECK(duos == 2);
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

TEST("broadcast: portrait canvas stacks guests and keeps screens 16:9 on top")
{
	const Rect v(0, 0, 1080, 1920);
	BroadcastSpec spec;
	for (const BroadcastTemplate &t : broadcastTemplates()) {
		const std::vector<Slot> s = buildBroadcast(t.key, v, spec);
		CHECK((int)s.size() == t.cameras + t.screens);
		for (size_t i = 0; i < s.size(); i++) {
			CHECK(inside(v, s[i].cell));
			CHECK(s[i].cell.w > 0 && s[i].cell.h > 0);
			for (size_t j = i + 1; j < s.size(); j++) {
				if (t.key.find("Pip") == std::string::npos && overlaps(s[i].cell, s[j].cell))
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

TEST("broadcast: camera-only 16:9 templates")
{
	const char *keys[] = {"Cam2Portrait",   "Cam2Square",     "Cam3Square",      "Cam2HostPip",  "Cam1Strip2",
			      "Cam1Strip3",     "Cam1Strip4",     "Cam1Strip5",      "Cam1Framed",   "Cam1Square",
			      "Cam1Portrait",   "Cam1TitleRight", "Cam1TitleBottom", "Cam1Cinema",   "Cam2Stacked",
			      "Cam2Wide",       "Cam2GuestPip",   "Cam2Cinema",      "Cam3Portrait", "Cam3HostPip2",
			      "Cam3HostColumn", "Cam3TwoOne",     "Cam3Rows",        "Cam4Portrait", "Cam4SquareRow",
			      "Cam4SquareGrid", "Cam4HostColumn", "Cam5Portrait",    "Cam5Square",   "Cam5HostColumn",
			      "Cam6Square",     "Cam6HostL",      "Cam6TwoFour"};
	const auto &all = broadcastTemplates();
	int found = 0;
	for (const char *key : keys) {
		const auto it =
			std::find_if(all.begin(), all.end(), [&](const BroadcastTemplate &t) { return t.key == key; });
		if (it == all.end()) {
			t::fail(__FILE__, __LINE__, std::string(key) + " missing");
			continue;
		}
		found++;
		CHECK(it->landscapeOnly && it->screens == 0 && it->chats == 0);
		const bool pip = it->key.find("Pip") != std::string::npos;
		/* 1080p, 720p and 4K canvases, with and without margin. */
		const Rect canvases[] = {kCanvas, Rect(0, 0, 1280, 720), Rect(100, 50, 3840, 2160)};
		for (const Rect &canvas : canvases) {
			for (double margin : {0.0, 40.0}) {
				BroadcastSpec spec;
				spec.margin = margin;
				const std::vector<Slot> s = buildBroadcast(key, canvas, spec);
				if ((int)s.size() != it->cameras)
					t::fail(__FILE__, __LINE__, std::string(key) + " camera count");
				int hosts = 0;
				for (size_t i = 0; i < s.size(); i++) {
					CHECK(s[i].kind == SlotKind::Camera);
					hosts += s[i].host ? 1 : 0;
					if (!inside(canvas, s[i].cell) || !(s[i].cell.w > 0 && s[i].cell.h > 0))
						t::fail(__FILE__, __LINE__, std::string(key) + " outside the canvas");
					for (size_t j = i + 1; j < s.size(); j++) {
						if (overlaps(s[i].cell, s[j].cell) && !pip)
							t::fail(__FILE__, __LINE__, std::string(key) + " overlap");
					}
				}
				CHECK(hosts == 1);
				/* Host first, except Cam2HostPip where the host is the inset on top. */
				CHECK(s.empty() || (it->key == "Cam2HostPip" ? s.back().host : s.front().host));
				/* Back to front: the inset (last) is smaller than what it covers. */
				if (pip && s.size() > 1)
					CHECK(s.back().cell.w * s.back().cell.h < s.front().cell.w * s.front().cell.h);
			}
		}
	}
	CHECK(found == 33);
	/* Cam1Strip<n>: one host and n guests. */
	CHECK(buildBroadcast("Cam1Strip5", kCanvas, {}).size() == 6);
}

TEST("broadcast: camera templates follow the review geometry")
{
	BroadcastSpec spec;
	spec.gap = 16;
	/* Host + strip: host 16:9 on 720 px, strip on the 1080p reference. */
	std::vector<Slot> s = buildBroadcast("Cam1Strip2", kCanvas, spec);
	CHECK(s[0].cell == Rect(320, 40, 1280, 720));
	CHECK_NEAR(s[1].cell.h, 250, 1e-9);
	CHECK_NEAR(s[1].cell.y, 790, 1e-9);
	/* Scaled to 720p: everything two thirds. */
	s = buildBroadcast("Cam1Strip2", Rect(0, 0, 1280, 720), spec);
	CHECK_NEAR(s[0].cell.x, 320 * 2.0 / 3.0, 1e-9);
	CHECK_NEAR(s[0].cell.h, 480, 1e-9);
	/* Picture in picture: 480 px wide, 40 px from the bottom right corner. */
	s = buildBroadcast("Cam2GuestPip", kCanvas, spec);
	CHECK(s[0].cell == kCanvas);
	CHECK(s[1].cell == Rect(1920 - 40 - 480, 1080 - 40 - 270, 480, 270));
	/* Host + L: the host spans 2 x 2 cells of a 3 x 3 grid. */
	s = buildBroadcast("Cam6HostL", kCanvas, spec);
	const double cw = (1920 - 32) / 3.0;
	CHECK_NEAR(s[0].cell.w, 2 * cw + 16, 1e-9);
	CHECK_NEAR(s[1].cell.x, 2 * (cw + 16), 1e-9);
	/* Margin: the content area shrinks, the cinema band keeps 2.39:1. */
	spec.margin = 60;
	s = buildBroadcast("Cam1Cinema", kCanvas, spec);
	CHECK_NEAR(s[0].cell.w / s[0].cell.h, 2.39, 1e-9);
	CHECK_NEAR(s[0].cell.x, 60, 1e-9);
}
