#include "test.hpp"

#include "portrait-map.hpp"

using namespace layouts::core;

namespace {
const Rect kWide{0, 0, 1920, 1080};
const Rect kTall{0, 0, 1080, 1920};

bool inside(const Rect &outer, const Rect &r)
{
	return r.x >= outer.x - 1e-6 && r.y >= outer.y - 1e-6 && r.right() <= outer.right() + 1e-6 &&
	       r.bottom() <= outer.bottom() + 1e-6;
}

bool overlaps(const Rect &a, const Rect &b)
{
	return a.x < b.right() - 1e-6 && b.x < a.right() - 1e-6 && a.y < b.bottom() - 1e-6 && b.y < a.bottom() - 1e-6;
}

std::vector<Slot> slotsOf(int cameras, int screens, int chats, bool ticker = false)
{
	std::vector<Slot> out;
	for (int i = 0; i < cameras; i++)
		out.push_back({SlotKind::Camera, {}});
	for (int i = 0; i < screens; i++)
		out.push_back({SlotKind::Screen, {}});
	for (int i = 0; i < chats; i++) {
		Slot s{SlotKind::Chat, {}};
		s.ticker = ticker;
		out.push_back(s);
	}
	return out;
}

/* Every twin link points to a slot of the same kind, each portrait slot is used once,
 * everything sits inside the canvas and nothing overlaps. */
void checkTwin(const std::vector<Slot> &land, const PortraitMap &m, bool allowOverlap, const std::string &what)
{
	if (m.twinOf.size() != land.size()) {
		t::fail(__FILE__, __LINE__, what + ": twin size");
		return;
	}
	std::vector<int> used(m.tiles.size(), 0);
	for (size_t i = 0; i < land.size(); i++) {
		const int j = m.twinOf[i];
		if (j < 0 || j >= (int)m.tiles.size()) {
			t::fail(__FILE__, __LINE__, what + ": missing twin");
			continue;
		}
		if (m.tiles[j].kind != land[i].kind)
			t::fail(__FILE__, __LINE__, what + ": kind mismatch");
		used[j]++;
	}
	for (int u : used)
		if (u != 1)
			t::fail(__FILE__, __LINE__, what + ": twin used " + std::to_string(u) + " times");
	for (const Slot &s : m.tiles) {
		if (!inside(kTall, s.cell) || !(s.cell.w > 0) || !(s.cell.h > 0))
			t::fail(__FILE__, __LINE__, what + ": cell outside or empty");
	}
	if (!allowOverlap) {
		for (size_t i = 0; i < m.tiles.size(); i++)
			for (size_t j = i + 1; j < m.tiles.size(); j++)
				if (overlaps(m.tiles[i].cell, m.tiles[j].cell))
					t::fail(__FILE__, __LINE__, what + ": overlap");
	}
}
} // namespace

TEST("portrait map: every legacy template maps to itself on the portrait canvas")
{
	BroadcastSpec spec;
	int legacy = 0;
	for (const BroadcastTemplate &t : broadcastTemplates()) {
		if (!supportsPortrait(t.key))
			continue;
		legacy++;
		const auto land = buildBroadcast(t.key, kWide, spec);
		const PortraitMap m = mapToPortrait(t.key, land, kTall, spec);
		CHECK(m.strategy == PortraitStrategy::Legacy);
		CHECK(m.key == t.key);
		checkTwin(land, m, t.key == "Screen1Cam1Pip", t.key);
	}
	CHECK(legacy >= 23);
}

TEST("portrait map: unknown layouts are generated from the counts")
{
	BroadcastSpec spec;
	for (int c = 0; c <= 6; c++)
		for (int s = 0; s <= 3; s++)
			for (int h = 0; h <= 2; h++) {
				if (c + s + h == 0)
					continue;
				const auto land = slotsOf(c, s, h);
				const PortraitMap m = mapToPortrait("Custom", land, kTall, spec);
				CHECK(m.strategy == PortraitStrategy::Generated);
				checkTwin(land, m, false,
					  "c" + std::to_string(c) + "s" + std::to_string(s) + "h" + std::to_string(h));
			}
}

TEST("portrait map: screens on top, guests in the middle, chat at the bottom")
{
	BroadcastSpec spec;
	spec.gap = 0;
	const auto land = slotsOf(2, 1, 1);
	const PortraitMap m = mapToPortrait("Screen1Cam2Chat", land, kTall, spec);
	CHECK(m.key == "Portrait/2c1s1h");
	const Rect cam0 = m.tiles[m.twinOf[0]].cell, cam1 = m.tiles[m.twinOf[1]].cell;
	const Rect screen = m.tiles[m.twinOf[2]].cell, chat = m.tiles[m.twinOf[3]].cell;
	CHECK_NEAR(screen.y, 0, 1e-6);
	CHECK_NEAR(screen.w, 1080, 1e-6);
	CHECK_NEAR(screen.h, 1080 * 9.0 / 16.0, 1e-6);
	CHECK(cam0.y >= screen.bottom() - 1e-6 && cam1.y >= screen.bottom() - 1e-6);
	CHECK(chat.y >= cam0.bottom() - 1e-6);
	CHECK_NEAR(chat.bottom(), 1920, 1e-6);
	CHECK_NEAR(chat.w, 1080, 1e-6);
}

TEST("portrait map: two guests alone stack, four make a grid")
{
	BroadcastSpec spec;
	spec.gap = 0;
	PortraitMap two = mapToPortrait("Custom", slotsOf(2, 0, 0), kTall, spec);
	CHECK_NEAR(two.tiles[0].cell.w, 1080, 1e-6);
	CHECK_NEAR(two.tiles[1].cell.y, 960, 1e-6);
	PortraitMap four = mapToPortrait("Custom", slotsOf(4, 0, 0), kTall, spec);
	CHECK_NEAR(four.tiles[0].cell.w, 540, 1e-6);
	CHECK_NEAR(four.tiles[3].cell.y, 960, 1e-6);
}

TEST("portrait map: the host gets the wide row and keeps its index")
{
	BroadcastSpec spec;
	spec.gap = 0;
	auto land = slotsOf(4, 0, 0);
	land[2].host = true;
	const PortraitMap m = mapToPortrait("Custom", land, kTall, spec);
	const Slot &h = m.tiles[m.twinOf[2]];
	CHECK(h.host);
	CHECK_NEAR(h.cell.y, 0, 1e-6);
	CHECK_NEAR(h.cell.w, 1080, 1e-6);
	for (int i : {0, 1, 3})
		CHECK(m.tiles[m.twinOf[i]].cell.y >= h.cell.bottom() - 1e-6);
}

TEST("portrait map: a ticker chat keeps a thin band, screens alone are centered")
{
	BroadcastSpec spec;
	spec.gap = 0;
	const PortraitMap t = mapToPortrait("X", slotsOf(1, 0, 1, true), kTall, spec);
	const Rect chat = t.tiles[t.twinOf[1]].cell;
	CHECK(chat.h < 1920 * 0.1);
	CHECK(t.tiles[t.twinOf[1]].ticker);
	const PortraitMap s = mapToPortrait("X", slotsOf(0, 1, 0), kTall, spec);
	const Rect screen = s.tiles[0].cell;
	CHECK_NEAR(screen.y + screen.h / 2, 960, 1e-6);
}

TEST("portrait map: landscape-only and degenerate inputs")
{
	BroadcastSpec spec;
	CHECK(!supportsPortrait("NoSuchTemplate"));
	CHECK(mapToPortrait("Cam2Split", {}, kTall, spec).tiles.empty());
	CHECK(mapToPortrait("Cam2Split", slotsOf(2, 0, 0), {0, 0, 0, 0}, spec).tiles.empty());
	/* Counts that do not match the template: generated instead. */
	const PortraitMap m = mapToPortrait("Cam2Split", slotsOf(3, 0, 0), kTall, spec);
	CHECK(m.strategy == PortraitStrategy::Generated);
	CHECK(m.tiles.size() == 3);
	/* Round bubbles carry over. */
	auto land = slotsOf(1, 0, 0);
	land[0].shape = SlotShape::Round;
	CHECK(mapToPortrait("Y", land, kTall, spec).tiles[0].shape == SlotShape::Round);
	/* Shortcut from a template key. */
	const PortraitMap k = mapTemplateToPortrait("Screen1Cam3Strip", kWide, kTall, spec);
	CHECK(k.strategy == PortraitStrategy::Legacy && k.tiles.size() == 4);
}
