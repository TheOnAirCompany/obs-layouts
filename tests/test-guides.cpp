#include "test.hpp"

#include "guides.hpp"

using namespace layouts::core;

TEST("guides: add, move, lock, remove")
{
	GuideDocument d;
	const uint64_t a = d.add("s1", "Scene 1", Axis::X, 960, false);
	const uint64_t b = d.add("s1", "Scene 1", Axis::Y, 540, true);
	CHECK(a != 0 && b != 0 && a != b);
	CHECK(d.sceneGuides("s1").size() == 1);
	CHECK(d.globalGuides().size() == 1);
	CHECK(d.visibleGuides("s1").size() == 2);
	CHECK(d.visibleGuides("other").size() == 1);

	CHECK(d.move(a, 100));
	CHECK_NEAR(d.find(a)->guide.pos, 100, 0);
	CHECK(d.setLocked(b, true));
	CHECK(d.find(b)->guide.locked);
	CHECK(d.remove(a));
	CHECK(!d.find(a));
	CHECK(!d.remove(a));
}

TEST("guides: invalid positions are refused")
{
	GuideDocument d;
	CHECK(d.add("s", "S", Axis::X, 1e9, false) == 0);
	CHECK(d.add("s", "S", Axis::X, std::nan(""), false) == 0);
	const uint64_t id = d.add("s", "S", Axis::X, 10, false);
	CHECK(!d.move(id, INFINITY));
}

TEST("guides: switch between scene and global")
{
	GuideDocument d;
	const uint64_t id = d.add("s1", "Scene 1", Axis::X, 10, false);
	CHECK(d.setGlobal(id, "s1", "Scene 1", true));
	CHECK(d.find(id)->global);
	CHECK(d.sceneGuides("s1").empty());
	CHECK(d.setGlobal(id, "s2", "Scene 2", false));
	CHECK(d.sceneGuides("s2").size() == 1);
}

TEST("guides: records round trip")
{
	GuideDocument d;
	d.add("u1", "Cam", Axis::X, 12.5, false);
	const uint64_t l = d.add("u1", "Cam", Axis::Y, 300, false);
	d.setLocked(l, true);
	d.add("", "", Axis::Y, 1000, true);

	const auto r = d.toRecords();
	CHECK(r.version == kGuidesSchemaVersion);
	GuideDocument e;
	CHECK(e.fromRecords(r) == 0);
	CHECK(e.sceneGuides("u1").size() == 2);
	CHECK(e.sceneGuides("u1")[1].locked);
	CHECK(e.globalGuides().size() == 1);
	CHECK_NEAR(e.sceneGuides("u1")[0].pos, 12.5, 0);
}

TEST("guides: v0 migration and rejection of bad entries")
{
	GuideDocument::Records r;
	r.version = 0;
	r.scenes.push_back({"u1", "Cam", {{"v", 10, false}, {"h", 20, true}, {"z", 5, false}, {"v", NAN, false}}});
	r.scenes.push_back({"", "", {{"v", 1, false}}});
	GuideDocument d;
	CHECK(d.fromRecords(r) == 3);
	CHECK(d.sceneGuides("u1").size() == 2);
	CHECK(d.sceneGuides("u1")[0].axis == Axis::X);
	CHECK(d.sceneGuides("u1")[1].axis == Axis::Y);
}

TEST("guides: future schema versions are read best effort")
{
	GuideDocument::Records r;
	r.version = 99;
	r.global.push_back({"x", 5, false});
	GuideDocument d;
	CHECK(d.fromRecords(r) == 0);
	CHECK(d.globalGuides().size() == 1);
}

TEST("guides: adopt by name and prune orphans")
{
	GuideDocument d;
	d.add("old-uuid", "Interview", Axis::X, 10, false);
	d.add("gone", "Deleted", Axis::X, 10, false);
	d.adoptByName("new-uuid", "Interview");
	CHECK(d.sceneGuides("old-uuid").empty());
	CHECK(d.sceneGuides("new-uuid").size() == 1);
	d.pruneOrphans({"new-uuid"});
	CHECK(d.scenes().size() == 1);
}
