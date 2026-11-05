#include "test.hpp"

#include "broadcast-inventive.hpp"

#include <algorithm>
#include <map>

using namespace layouts::core;

namespace {
const Rect kCanvas{0, 0, 1920, 1080};

const std::vector<std::string> kKeys = {
	"ChatCam",   "ChatScreenCam", "ChatDuo",      "ChatCard",     "ChatTicker", "ChatGuests", "ChatQuestions",
	"ChatVisio", "ChatPodcast",   "ChatGameplay", "TiltedCards",  "Polaroid",   "Bento4",     "Bento6",
	"Dock",      "Diagonal",      "CardStack",    "TiltedScreen", "Masonry",    "PortHole",
};

bool inside(const Rect &outer, const Rect &r)
{
	return r.x >= outer.x - 1e-6 && r.y >= outer.y - 1e-6 && r.right() <= outer.right() + 1e-6 &&
	       r.bottom() <= outer.bottom() + 1e-6;
}

std::vector<std::string> keysFor(int cams, int screens, int chats, bool portrait)
{
	std::vector<std::string> out;
	for (const BroadcastTemplate &t : templatesFor(cams, screens, chats, portrait))
		out.push_back(t.key);
	return out;
}

bool has(const std::vector<std::string> &v, const std::string &k)
{
	return std::find(v.begin(), v.end(), k) != v.end();
}

std::vector<double> rotations(const std::string &key)
{
	std::vector<double> out;
	for (const Slot &s : buildBroadcast(key, kCanvas, {}))
		out.push_back(s.rotation);
	return out;
}
} // namespace

TEST("inventive: the 20 templates are listed, landscape only, slot counts match")
{
	std::map<std::string, BroadcastTemplate> byKey;
	for (const BroadcastTemplate &t : broadcastTemplates())
		if (t.landscapeOnly)
			byKey.emplace(t.key, t);
	CHECK(byKey.size() == kKeys.size());
	for (const std::string &k : kKeys) {
		if (!byKey.count(k)) {
			t::fail(__FILE__, __LINE__, k + " missing");
			continue;
		}
		const BroadcastTemplate &t = byKey.at(k);
		const std::vector<Slot> slots = buildBroadcast(k, kCanvas, {});
		int cams = 0, screens = 0, chats = 0, hosts = 0;
		for (const Slot &s : slots) {
			(s.kind == SlotKind::Camera ? cams : s.kind == SlotKind::Screen ? screens : chats)++;
			if (s.host) {
				hosts++;
				CHECK(s.kind == SlotKind::Camera);
			}
			CHECK(inside(kCanvas, s.cell));
			CHECK(s.cell.w > 0 && s.cell.h > 0);
			CHECK((s.shape == SlotShape::Polygon) == !s.polygon.empty());
			CHECK(!s.ticker || s.kind == SlotKind::Chat);
		}
		if (cams != t.cameras || screens != t.screens || chats != t.chats || hosts != 1)
			t::fail(__FILE__, __LINE__, k + " slot counts");
	}
	/* Chat counts per key. */
	CHECK(byKey.at("ChatCam").chats == 1 && byKey.at("Bento6").chats == 1 && byKey.at("ChatVisio").chats == 1);
	CHECK(byKey.at("Bento4").chats == 0 && byKey.at("Dock").chats == 0);
	CHECK(byKey.at("ChatVisio").cameras == 3 && byKey.at("ChatVisio").screens == 1);
	CHECK(byKey.at("Bento6").cameras == 4 && byKey.at("Bento6").screens == 1);
	CHECK(byKey.at("Masonry").cameras == 5);
	CHECK(byKey.at("Dock").cameras == 4 && byKey.at("Dock").screens == 1);
}

TEST("inventive: rotations, frames and tickers")
{
	CHECK(rotations("TiltedCards") == std::vector<double>({-6, 6, 0}));
	CHECK(rotations("Polaroid") == std::vector<double>({-4, 3}));
	CHECK(rotations("CardStack") == std::vector<double>({0, 5, -3, 2}));
	CHECK(rotations("TiltedScreen") == std::vector<double>({-3, 0}));
	CHECK(rotations("Bento4") == std::vector<double>({0, 0, 0, 0}));
	for (const Slot &s : buildBroadcast("Polaroid", kCanvas, {}))
		CHECK(s.frame);
	/* TiltedCards: the host is the front card, drawn last (back to front). */
	CHECK(buildBroadcast("TiltedCards", kCanvas, {}).back().host);
	const std::vector<Slot> ticker = buildBroadcast("ChatTicker", kCanvas, {});
	CHECK(ticker.back().kind == SlotKind::Chat && ticker.back().ticker);
	CHECK(ticker.back().cell == Rect(0, 960, 1920, 120));
	CHECK(buildBroadcast("ChatPodcast", kCanvas, {}).back().ticker);
	CHECK(!buildBroadcast("ChatCam", kCanvas, {}).back().ticker);
}

TEST("inventive: round and polygon shapes")
{
	auto shapes = [](const std::string &key) {
		std::vector<SlotShape> out;
		for (const Slot &s : buildBroadcast(key, kCanvas, {}))
			out.push_back(s.shape);
		return out;
	};
	const SlotShape R = SlotShape::Rect, O = SlotShape::Round, P = SlotShape::Polygon;
	CHECK(shapes("Dock") == std::vector<SlotShape>({R, O, O, O, O}));
	CHECK(shapes("ChatGameplay") == std::vector<SlotShape>({R, O, R}));
	CHECK(shapes("TiltedScreen") == std::vector<SlotShape>({R, O}));
	CHECK(shapes("PortHole") == std::vector<SlotShape>({O, R}));
	CHECK(shapes("Diagonal") == std::vector<SlotShape>({P, P}));
	const std::vector<Slot> diag = buildBroadcast("Diagonal", kCanvas, {});
	CHECK(diag[0].polygon.size() == 4 && diag[1].polygon.size() == 4);
	CHECK_NEAR(diag[0].polygon[2].x, 820.0 / 1100.0, 1e-3);
	CHECK_NEAR(diag[1].polygon[0].x, 296.0 / 1100.0, 1e-3);
	for (const Slot &s : buildBroadcast("Dock", kCanvas, {}))
		if (s.shape == SlotShape::Round)
			CHECK_NEAR(s.cell.w, s.cell.h, 1e-9);
}

TEST("inventive: templatesFor matches chats exactly and drops landscape-only on portrait")
{
	CHECK(keysFor(1, 0, 1, false) == std::vector<std::string>({"ChatCam", "ChatCard"}));
	CHECK(keysFor(1, 0, 1, true).empty());
	CHECK(keysFor(2, 0, 1, false) == std::vector<std::string>({"ChatDuo", "ChatQuestions", "ChatPodcast"}));
	CHECK(keysFor(1, 1, 1, false) == std::vector<std::string>({"ChatScreenCam", "ChatTicker", "ChatGameplay"}));
	CHECK(keysFor(4, 0, 1, false) == std::vector<std::string>({"ChatGuests"}));
	CHECK(keysFor(3, 1, 1, false) == std::vector<std::string>({"ChatVisio"}));
	CHECK(keysFor(4, 1, 1, false) == std::vector<std::string>({"Bento6"}));
	CHECK(keysFor(1, 0, 2, false).empty());
	/* Without chats, no chat template. */
	const std::vector<std::string> duo = keysFor(2, 0, 0, false);
	CHECK(duo.size() == 4 && has(duo, "Polaroid") && has(duo, "Diagonal") && !has(duo, "ChatDuo"));
	CHECK(keysFor(2, 0, 0, true).size() == 2);
	const std::vector<std::string> one = keysFor(1, 1, 0, false);
	CHECK(has(one, "Screen1Cam1Pip") && has(one, "TiltedScreen") && has(one, "PortHole") && one.size() == 4);
	CHECK(!has(keysFor(1, 1, 0, true), "PortHole"));
	CHECK(has(keysFor(3, 0, 0, false), "TiltedCards"));
	CHECK(has(keysFor(4, 0, 0, false), "CardStack"));
	CHECK(has(keysFor(5, 0, 0, false), "Masonry"));
	CHECK(has(keysFor(3, 1, 0, false), "Bento4"));
	CHECK(has(keysFor(4, 1, 0, false), "Dock"));
	/* Default arguments: no chat, landscape. */
	CHECK(templatesFor(2, 0).size() == 4);
}

TEST("inventive: scaled to the canvas, margin applied, bubbles stay round")
{
	std::vector<Slot> out;
	CHECK(!buildInventiveTemplate("Cam2Split", kCanvas, {}, out) && out.empty());
	CHECK(buildInventiveTemplate("PortHole", Rect(0, 0, 1280, 720), {}, out));
	CHECK(out.size() == 2);
	CHECK_NEAR(out[0].cell.x, 80.0 * 2 / 3, 1e-9);
	CHECK_NEAR(out[0].cell.w, 680.0 * 2 / 3, 1e-9);
	CHECK_NEAR(out[1].cell.right(), 1880.0 * 2 / 3, 1e-9);

	BroadcastSpec spec;
	spec.margin = 40;
	const std::vector<Slot> card = buildBroadcast("ChatCard", kCanvas, spec);
	CHECK(card[0].cell == Rect(40, 40, 1840, 1000)); /* full camera inside the margin */

	/* A 4:3 canvas stretches the rects but keeps the bubbles round. */
	const Rect c43(0, 0, 1440, 1080);
	for (const Slot &s : buildBroadcast("Dock", c43, {})) {
		if (s.shape == SlotShape::Round)
			CHECK_NEAR(s.cell.w, s.cell.h, 1e-9);
		CHECK(inside(c43, s.cell));
	}
}
