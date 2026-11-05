#include "test.hpp"

#include "broadcast.hpp"
#include "broadcast-screens.hpp"

#include <set>

#include <algorithm>

using namespace layouts::core;

namespace {
const Rect kCanvas{0, 0, 1920, 1080};

bool inside(const Rect &outer, const Rect &r)
{
	return r.x >= outer.x - 1e-6 && r.y >= outer.y - 1e-6 && r.right() <= outer.right() + 1e-6 &&
	       r.bottom() <= outer.bottom() + 1e-6;
}

/* Templates that overlap on purpose: a picture in picture, a bubble, a floating screen. */
bool overlayTemplate(const std::string &key)
{
	static const std::set<std::string> keys = {
		"Screen1Cam1Pip", "Screen1Cam2Pip", "Screen2Pip",     "Screen1Cam1Bubble", "Screen1Cam1Over",
		"Screen1Cam3Pip", "Screen1Cam4Pip", "Screen2Cam1Pip", "Screen2Cam2Pip",
	};
	return keys.count(key) > 0;
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
		/* No overlap, except the picture-in-picture templates. */
		if (!overlayTemplate(t.key)) {
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
				if (!overlayTemplate(t.key) && overlaps(s[i].cell, s[j].cell))
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

TEST("broadcast: 0.5 screen templates, counts per key")
{
	struct Want {
		const char *key;
		int cameras, screens;
	};
	const Want wants[] = {
		{"Screen1Cam1Float", 1, 1},
		{"Screen1Cam2Pip", 2, 1},
		{"Screen1Cam2Sides", 2, 1},
		{"Screen1Cam1Half", 1, 1},
		{"Screen3", 0, 3},
		{"Screen4Grid", 0, 4},
		{"Screen1Framed", 0, 1},
		{"Screen2Stacked", 0, 2},
		{"Screen2BigSmall", 0, 2},
		{"Screen2Pip", 0, 2},
		{"Screen3Row", 0, 3},
		{"Screen3TwoOne", 0, 3},
		{"Screen4Strip", 0, 4},
		{"Screen4Column", 0, 4},
		{"Screen1Cam1Portrait", 1, 1},
		{"Screen1Cam1Bubble", 1, 1},
		{"Screen1Cam1Over", 1, 1},
		{"Screen1Cam1Stack", 1, 1},
		{"Screen1Cam2Square", 2, 1},
		{"Screen1Cam2HostGuest", 2, 1},
		{"Screen1Cam3Pip", 3, 1},
		{"Screen1Cam3Float", 3, 1},
		{"Screen1Cam4Pip", 4, 1},
		{"Screen1Cam4Grid", 4, 1},
		{"Screen1Cam4Sides", 4, 1},
		{"Screen1Cam4L", 4, 1},
		{"Screen1Cam5L", 5, 1},
		{"Screen1Cam6L", 6, 1},
		{"Screen1Cam6Sides", 6, 1},
		{"Screen1Cam6Grid", 6, 1},
		{"Screen2Cam1", 1, 2},
		{"Screen2Cam1Pip", 1, 2},
		{"Screen2Cam1Side", 1, 2},
		{"Screen2Cam2Pip", 2, 2},
		{"Screen2Cam2Sides", 2, 2},
		{"Screen2Cam3", 3, 2},
		{"Screen2Cam4", 4, 2},
		{"Screen3Cam1", 1, 3},
		{"Screen3Cam2", 2, 3},
		{"Screen3Cam3", 3, 3},
		{"Screen4Cam1", 1, 4},
		{"Screen4Cam2", 2, 4},
	};
	std::vector<BroadcastTemplate> list;
	appendScreenTemplates(list);
	CHECK(list.size() == 42);
	CHECK(sizeof(wants) / sizeof(wants[0]) == 42);
	for (const Want &w : wants) {
		bool listed = false;
		for (const BroadcastTemplate &t : broadcastTemplates()) {
			if (t.key == w.key) {
				listed = true;
				CHECK(t.landscapeOnly);
				CHECK(t.cameras == w.cameras && t.screens == w.screens);
			}
		}
		if (!listed)
			t::fail(__FILE__, __LINE__, std::string(w.key) + " not listed");
		for (const Rect &canvas : {kCanvas, Rect(100, 50, 3840, 2160), Rect(0, 0, 1280, 720)}) {
			BroadcastSpec spec;
			spec.margin = canvas.w > 2000 ? 40 : 0;
			const std::vector<Slot> slots = buildBroadcast(w.key, canvas, spec);
			int cams = 0, screens = 0, hosts = 0;
			bool screenSeen = false;
			for (const Slot &s : slots) {
				if (s.kind == SlotKind::Camera) {
					cams++;
					if (screenSeen)
						t::fail(__FILE__, __LINE__,
							std::string(w.key) + " camera after a screen");
				} else {
					screens++;
					screenSeen = true;
				}
				hosts += s.host ? 1 : 0;
				if (!inside(canvas, s.cell) || !(s.cell.w > 0 && s.cell.h > 0))
					t::fail(__FILE__, __LINE__, std::string(w.key) + " cell outside the canvas");
			}
			if (cams != w.cameras || screens != w.screens)
				t::fail(__FILE__, __LINE__, std::string(w.key) + " slot counts");
			/* One host: the first camera. */
			CHECK(hosts == (w.cameras > 0 ? 1 : 0));
			if (w.cameras > 0)
				CHECK(slots[0].host);
		}
	}
}

TEST("broadcast: 0.5 screen templates keep screens 16:9")
{
	for (double gap : {0.0, 8.0, 24.0}) {
		BroadcastSpec spec;
		spec.gap = gap;
		std::vector<BroadcastTemplate> list;
		appendScreenTemplates(list);
		for (const BroadcastTemplate &t : list) {
			/* Cells the page does not fit (the screen is fitted inside when applied): the
			 * 50/50 split drawn 900 x 518 and the column of Screen4Column. */
			if (t.key == "Screen1Cam1Half")
				continue;
			bool first = true;
			for (const Slot &s : buildBroadcast(t.key, kCanvas, spec)) {
				if (s.kind != SlotKind::Screen)
					continue;
				if (!first && t.key == "Screen4Column")
					continue;
				first = false;
				const double a = s.cell.w / s.cell.h;
				if (std::abs(a / (16.0 / 9.0) - 1) > 0.01)
					t::fail(__FILE__, __LINE__, t.key + " screen not 16:9");
			}
		}
	}
}

TEST("broadcast: 0.5 screen templates follow the page geometry")
{
	BroadcastSpec spec;
	spec.gap = 16; /* the page gap */
	auto s = buildBroadcast("Screen1Cam1Float", kCanvas, spec);
	CHECK(s[0].cell == Rect(1530, 615, 330, 330));
	CHECK(s[1].cell == Rect(60, 135, 1440, 810));
	s = buildBroadcast("Screen1Cam4L", kCanvas, spec);
	CHECK(s.back().cell == Rect(0, 0, 1424, 801));
	CHECK(s[0].cell == Rect(1440, 0, 480, 392.5));
	CHECK_NEAR(s[2].cell.y, 817, 1e-9);
	s = buildBroadcast("Screen2Cam1Side", kCanvas, spec);
	CHECK_NEAR(s[0].cell.x, 976, 1e-9);
	/* The bubble is round and square. */
	s = buildBroadcast("Screen1Cam1Bubble", kCanvas, spec);
	CHECK(s[0].shape == SlotShape::Round);
	CHECK(s[0].cell == Rect(1580, 740, 300, 300));
	/* Uniform scale: a 4K canvas doubles the drawing, the gap stays in canvas pixels. */
	spec.gap = 32;
	const auto big = buildBroadcast("Screen1Cam4L", Rect(0, 0, 3840, 2160), spec);
	CHECK(big[0].cell == Rect(2880, 0, 960, 785));
	/* A margin shrinks the drawing uniformly (scale 0.9 here) and centers it. */
	spec.gap = 16;
	spec.margin = 54;
	s = buildBroadcast("Screen1Cam1Over", kCanvas, spec);
	CHECK_NEAR(s[0].cell.x, 96, 1e-9);
	CHECK_NEAR(s[0].cell.y, 54, 1e-9);
	CHECK_NEAR(s[0].cell.w, 1728, 1e-9);
	CHECK_NEAR(s[0].cell.h, 972, 1e-9);
}

TEST("broadcast: overlapping slots stack small over large")
{
	BroadcastSpec spec;
	auto cells = [&](const std::string &key) {
		std::vector<Rect> out;
		for (const Slot &s : buildBroadcast(key, kCanvas, spec))
			out.push_back(s.cell);
		return out;
	};
	/* Screen floating over the camera: the screen (index 1) comes to the front. */
	CHECK(overlayOrder(cells("Screen1Cam1Over")) == std::vector<size_t>{1});
	CHECK(overlayOrder(cells("Screen1Cam1Pip")) == std::vector<size_t>{0});
	CHECK(overlayOrder(cells("Screen2Pip")) == std::vector<size_t>{1});
	CHECK(overlayOrder(cells("Screen1Cam2Pip")).size() == 2);
	CHECK(overlayOrder(cells("Screen1Cam4L")).empty());
	CHECK(overlayOrder(cells("Cam4Grid")).empty());
}
