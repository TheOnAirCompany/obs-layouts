#include "test.hpp"

#include "user-layouts.hpp"

#include <algorithm>

using namespace layouts::core;

namespace {

const Rect kCanvas{0, 0, 1920, 1080};

UserLayout sample()
{
	UserLayout l;
	l.name = "Duo \"tilted\"";
	Slot screen{SlotKind::Screen, {960, 0, 960, 540}};
	Slot guest{SlotKind::Camera, {0, 540, 960, 540}};
	guest.rotation = -4;
	guest.frame = true;
	Slot host{SlotKind::Camera, {0, 0, 960, 540}};
	host.host = true;
	host.shape = SlotShape::Round;
	Slot cut{SlotKind::Camera, {960, 540, 960, 540}};
	cut.shape = SlotShape::Polygon;
	cut.polygon = {{0.2, 0}, {1, 0}, {0.8, 1}, {0, 1}};
	Slot chat{SlotKind::Chat, {0, 1000, 1920, 80}};
	chat.ticker = true;
	l.tiles = normalizeSlots({chat, screen, guest, cut, host}, kCanvas);
	return l;
}

struct RegistryReset {
	RegistryReset() { UserLayoutRegistry::instance().setAll({}); }
	~RegistryReset() { UserLayoutRegistry::instance().setAll({}); }
};

} // namespace

TEST("user layouts: slots are normalized in reading order, hosts first")
{
	const UserLayout l = sample();
	CHECK(l.tiles.size() == 5);
	CHECK(l.tiles[0].host);
	CHECK(l.tiles[1].kind == SlotKind::Camera && l.tiles[1].rotation == -4); /* row 2, left */
	CHECK(l.tiles[2].kind == SlotKind::Camera && l.tiles[2].shape == SlotShape::Polygon);
	CHECK(l.tiles[3].kind == SlotKind::Screen);
	CHECK(l.tiles[4].kind == SlotKind::Chat);
	CHECK_NEAR(l.tiles[3].cell.x, 0.5, 1e-9);
	CHECK_NEAR(l.tiles[4].cell.h, 80.0 / 1080, 1e-9);
	CHECK(l.cameras() == 3 && l.screens() == 1 && l.chats() == 1);
}

TEST("user layouts: built on any canvas size")
{
	const UserLayout l = sample();
	const std::vector<Slot> s = buildUserLayout(l, {0, 0, 1280, 720});
	CHECK(s.size() == 5);
	CHECK_NEAR(s[3].cell.x, 640, 1e-6);
	CHECK_NEAR(s[3].cell.w, 640, 1e-6);
	CHECK_NEAR(s[1].cell.y, 360, 1e-6);
	CHECK(buildUserLayout(l, {0, 0, 0, 720}).empty());
}

TEST("user layouts: JSON round trip keeps every field")
{
	UserLayout l = sample();
	l.key = "user:00c0ffee";
	const std::string json = userLayoutsToJson({l});
	std::vector<UserLayout> back;
	CHECK(userLayoutsFromJson(json, back));
	CHECK(back.size() == 1);
	const UserLayout &b = back[0];
	CHECK(b.key == l.key && b.name == l.name);
	CHECK_NEAR(b.aspect, 16.0 / 9.0, 1e-5);
	CHECK(b.tiles.size() == l.tiles.size());
	for (size_t i = 0; i < b.tiles.size() && i < l.tiles.size(); i++) {
		CHECK(b.tiles[i].kind == l.tiles[i].kind);
		CHECK(b.tiles[i].shape == l.tiles[i].shape);
		CHECK(b.tiles[i].host == l.tiles[i].host);
		CHECK(b.tiles[i].frame == l.tiles[i].frame);
		CHECK(b.tiles[i].ticker == l.tiles[i].ticker);
		CHECK(b.tiles[i].polygon.size() == l.tiles[i].polygon.size());
		CHECK_NEAR(b.tiles[i].rotation, l.tiles[i].rotation, 1e-6);
		CHECK_NEAR(b.tiles[i].cell.x, l.tiles[i].cell.x, 1e-5);
		CHECK_NEAR(b.tiles[i].cell.w, l.tiles[i].cell.w, 1e-5);
	}
	CHECK(userLayoutsToJson(back) == json);
}

TEST("user layouts: reader ignores unknown fields and rejects bad input")
{
	const std::string json = R"({"format":"layouts-user-layout/1","app":{"v":[1,2,null]},
		"layouts":[{"key":"user:1","name":"Écran 😀","extra":true,
		  "slots":[{"kind":"camera","x":0,"y":0,"w":1,"h":1,"future":"x"},
		           {"kind":"hologram","x":0,"y":0,"w":1,"h":1},
		           {"kind":"screen","x":0,"y":0,"w":0,"h":1}]},
		  {"key":"oops","name":"empty","slots":[]}, 42]})";
	std::vector<UserLayout> out;
	CHECK(userLayoutsFromJson(json, out));
	CHECK(out.size() == 1);
	CHECK(out[0].tiles.size() == 1);
	CHECK(out[0].name == "\xC3\x89"
			     "cran \xF0\x9F\x98\x80");

	std::vector<UserLayout> none;
	std::string err;
	CHECK(!userLayoutsFromJson("{\"format\":\"layouts-user-layout/2\",\"layouts\":[]}", none, &err));
	CHECK(!err.empty());
	CHECK(!userLayoutsFromJson("{\"format\":\"something-else/1\"}", none));
	CHECK(!userLayoutsFromJson("{\"format\":\"layouts-user-layout/1\",\"layouts\":[", none));
	CHECK(!userLayoutsFromJson("", none));
	CHECK(none.empty());
}

TEST("user layouts: registry feeds templatesFor and buildBroadcast")
{
	RegistryReset reset;
	UserLayoutRegistry &reg = UserLayoutRegistry::instance();
	const size_t builtIn = templatesFor(3, 1, 1).size();
	const std::string key = reg.add(sample());
	CHECK(isUserKey(key));
	const std::vector<BroadcastTemplate> list = templatesFor(3, 1, 1);
	CHECK(list.size() == builtIn + 1);
	CHECK(list.back().key == key && list.back().chats == 1);
	CHECK(std::none_of(broadcastTemplates().begin(), broadcastTemplates().end(),
			   [&key](const BroadcastTemplate &t) { return t.key == key; }));
	CHECK(buildBroadcast(key, kCanvas, {}).size() == 5);
	CHECK(buildBroadcast("user:missing", kCanvas, {}).empty());

	CHECK(reg.rename(key, "Show"));
	CHECK(reg.find(key)->name == "Show");
	CHECK(reg.remove(key));
	CHECK(!reg.remove(key));
	CHECK(templatesFor(3, 1).size() == builtIn);
}

TEST("user layouts: merge skips duplicates and renames conflicts")
{
	RegistryReset reset;
	UserLayoutRegistry &reg = UserLayoutRegistry::instance();
	UserLayout a = sample();
	a.name = "Duo";
	const std::string key = reg.add(a);
	a.key = key;

	CHECK(reg.merge({a}) == 0); /* the same layout again */
	CHECK(reg.all().size() == 1);

	UserLayout changed = a;
	changed.tiles.pop_back();
	CHECK(reg.merge({changed}) == 1); /* same key, other content: new key, new name */
	CHECK(reg.all().size() == 2);
	CHECK(reg.all()[1].key != key);
	CHECK(reg.all()[1].name == "Duo (2)");

	UserLayout fresh = sample();
	fresh.name = "";
	CHECK(reg.merge({fresh}) == 1);
	CHECK(reg.all()[2].name == "Layout");
	CHECK(isUserKey(reg.all()[2].key));
}
