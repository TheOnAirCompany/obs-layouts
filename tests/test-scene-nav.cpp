/*
Layouts for OBS
Copyright (C) 2026 The OnAir Company

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "test.hpp"

#include "live.hpp"
#include "scene-nav.hpp"

using namespace layouts::core;
using namespace layouts::core::nav;

namespace {

/* What Stacks publishes for the demo show. */
const char *kOrg = R"({"schema_version":1,"tree":[
 {"type":"stack","name":"Ouverture","color":"#F76B15","children":[
   {"type":"scene","uuid":"u-count","name":"Countdown"},
   {"type":"scene","uuid":"u-open","name":"Opening"}]},
 {"type":"separator","label":"Plateau"},
 {"type":"stack","name":"Plateau","color":"#3E63DD","collapsed":true,"children":[
   {"type":"scene","uuid":"u-duo","name":"Duo","favorite":true},
   {"type":"scene","uuid":"u-gone","name":"Old","missing_since":1700000000},
   {"type":"stack","name":"Invités","children":[
     {"type":"scene","uuid":"u-solo","name":"Solo invité"}]}]},
 {"type":"stack","name":"Habillage","children":[]},
 {"type":"scene","uuid":"u-brb","name":"BRB","favorite":true},
 {"type":"future","name":"ignored"}
]})";

std::vector<CanvasInfo> canvases()
{
	CanvasInfo main{"c-main", "Main", true, 1920, 1080, {}};
	main.scenes = {{"u-open", "Opening"}, {"u-count", "Countdown"}, {"u-duo", "Duo"},
		       {"u-solo", "Solo invité"}, {"u-brb", "BRB"},         {"u-new", "Interview"}};
	CanvasInfo vertical{"c-vert", "Vertical", false, 1080, 1920, {}};
	vertical.scenes = {{"v-duo", "Duo ▯"}, {"v-solo", "Solo 9:16"}};
	/* Listed first: the main canvas still comes first. */
	return {vertical, main};
}

const Node *child(const Node &n, const std::string &name)
{
	for (const Node &c : n.children)
		if (c.name == name)
			return &c;
	return nullptr;
}

} // namespace

TEST("nav: parse the Stacks organization")
{
	std::vector<OrgNode> org;
	CHECK(parseOrganization(kOrg, org));
	CHECK(org.size() == 4); /* separator and unknown node dropped */
	CHECK(org[0].kind == OrgNode::Kind::Stack && org[0].name == "Ouverture" && org[0].color == "#F76B15");
	CHECK(org[1].collapsed && org[1].children.size() == 2); /* tombstone dropped */
	CHECK(org[1].children[0].favorite);
	CHECK(org[1].children[1].name == "Invités");
	CHECK(org[3].kind == OrgNode::Kind::Scene && org[3].uuid == "u-brb");

	CHECK(!parseOrganization("", org));
	CHECK(!parseOrganization("{not json", org));
	CHECK(!parseOrganization(R"({"schema_version":2,"tree":[]})", org));
	CHECK(parseOrganization(R"({"tree":[]})", org) && org.empty());
}

TEST("nav: tree with Stacks, main canvas first, unfiled scenes at the end")
{
	std::vector<OrgNode> org;
	parseOrganization(kOrg, org);
	const std::vector<Node> tree = buildTree(canvases(), &org);
	CHECK(tree.size() == 2);
	const Node &main = tree[0];
	CHECK(main.kind == Node::Kind::Canvas && main.main && main.ratio == "16:9");
	CHECK(main.orientation == Ratio::Landscape);
	CHECK(main.children.size() == 5); /* 3 stacks, BRB, Interview */
	CHECK(main.children[0].name == "Ouverture" && main.children[0].children.size() == 2);
	/* Stacks' order, not OBS order. */
	CHECK(main.children[0].children[0].name == "Countdown");
	const Node *plateau = child(main, "Plateau");
	CHECK(plateau && plateau->collapsed && plateau->color == "#3E63DD" && plateau->path == "Plateau");
	CHECK(plateau && sceneCount(*plateau) == 2);
	const Node *guests = plateau ? child(*plateau, "Invités") : nullptr;
	CHECK(guests && guests->path == "Plateau / Invités" && guests->children.size() == 1);
	const Node *habillage = child(main, "Habillage");
	CHECK(habillage && habillage->children.empty()); /* empty stacks keep their place */
	CHECK(main.children[3].name == "BRB" && main.children[3].favorite);
	CHECK(main.children[4].name == "Interview" && main.children[4].uuid == "u-new");
	CHECK(sceneCount(main) == 6);

	const Node &vert = tree[1];
	CHECK(vert.ratio == "9:16" && vert.orientation == Ratio::Portrait && !vert.main);
	CHECK(vert.children.size() == 2 && vert.children[0].kind == Node::Kind::Scene);
	CHECK(vert.children[0].canvas == "c-vert");

	const std::vector<std::string> paths = stackPaths(tree);
	CHECK(paths == (std::vector<std::string>{"Ouverture", "Plateau", "Plateau / Invités", "Habillage"}));
}

TEST("nav: without Stacks, flat lists in OBS order")
{
	const std::vector<Node> tree = buildTree(canvases(), nullptr);
	CHECK(tree.size() == 2);
	CHECK(tree[0].children.size() == 6);
	CHECK(tree[0].children[0].name == "Opening");
	for (const Node &n : tree[0].children)
		CHECK(n.kind == Node::Kind::Scene && !n.favorite);
	CHECK(stackPaths(tree).empty());
}

TEST("nav: organization matched by name when uuids changed")
{
	std::vector<OrgNode> org;
	parseOrganization(R"({"schema_version":1,"tree":[{"type":"stack","name":"S","children":[
		{"type":"scene","uuid":"old-uuid","name":"Duo"},
		{"type":"scene","uuid":"u-duo","name":"Duo"}]}]})",
			  org);
	const std::vector<Node> tree = buildTree(canvases(), &org);
	/* The first entry claims Duo by name; the second (same uuid) finds it taken. */
	CHECK(tree[0].children[0].children.size() == 1);
	CHECK(sceneCount(tree[0]) == 6);
}

TEST("nav: search keeps the parents of what matches")
{
	std::vector<OrgNode> org;
	parseOrganization(kOrg, org);
	const std::vector<Node> tree = buildTree(canvases(), &org);
	Filter f;
	CHECK(!f.active());
	CHECK(filterTree(tree, f).size() == 2);

	f.query = "INVITE"; /* case and accents fold */
	std::vector<Node> got = filterTree(tree, f);
	CHECK(got.size() == 1 && got[0].main);
	CHECK(got[0].children.size() == 1 && got[0].children[0].name == "Plateau");
	CHECK(got[0].children[0].children.size() == 1 && got[0].children[0].children[0].name == "Invités");
	CHECK(sceneCount(got[0]) == 1);

	f.query = "duo"; /* both canvases */
	got = filterTree(tree, f);
	CHECK(got.size() == 2 && sceneCount(got[0]) == 1 && sceneCount(got[1]) == 1);

	f.query = "nothing like this";
	CHECK(filterTree(tree, f).empty());
}

TEST("nav: ratio and favorites filters")
{
	std::vector<OrgNode> org;
	parseOrganization(kOrg, org);
	const std::vector<Node> tree = buildTree(canvases(), &org);
	Filter f;
	f.ratio = RatioFilter::Portrait;
	std::vector<Node> got = filterTree(tree, f);
	CHECK(got.size() == 1 && got[0].name == "Vertical" && got[0].children.size() == 2);

	f.ratio = RatioFilter::Landscape;
	got = filterTree(tree, f);
	CHECK(got.size() == 1 && got[0].main && got[0].children.size() == 5); /* empty stack kept */

	f.ratio = RatioFilter::Favorites;
	got = filterTree(tree, f);
	CHECK(got.size() == 1 && sceneCount(got[0]) == 2);
	CHECK(got[0].children.size() == 2 && got[0].children[0].name == "Plateau" && got[0].children[1].name == "BRB");

	f.query = "brb";
	got = filterTree(tree, f);
	CHECK(got.size() == 1 && sceneCount(got[0]) == 1 && got[0].children[0].name == "BRB");
}

TEST("nav: ratio labels")
{
	CHECK(ratioLabel(1920, 1080) == "16:9");
	CHECK(ratioLabel(1080, 1920) == "9:16");
	CHECK(ratioLabel(1280, 720) == "16:9");
	CHECK(ratioLabel(1366, 768) == "16:9"); /* within 1 % */
	CHECK(ratioLabel(1440, 1080) == "4:3");
	CHECK(ratioLabel(1080, 1080) == "1:1");
	CHECK(ratioLabel(1080, 1350) == "4:5");
	CHECK(ratioLabel(2560, 1080) == "2.37:1");
	CHECK(ratioLabel(1000, 333) == "3.00:1");
	CHECK(ratioLabel(0, 1080).empty());
	CHECK(ratioOf(1080, 1080) == Ratio::Square);
}

TEST("nav: tally of scenes and stacks")
{
	std::vector<OrgNode> org;
	parseOrganization(kOrg, org);
	const std::vector<Node> tree = buildTree(canvases(), &org);
	const std::set<std::string> program{"u-solo"}, preview{"u-duo", "u-count"};
	const Node &main = tree[0];
	const Node *plateau = child(main, "Plateau");
	CHECK(plateau && tallyOf(*plateau, program, preview) == Tally::Program); /* program wins */
	CHECK(tallyOf(main.children[0], program, preview) == Tally::Preview);   /* Ouverture */
	CHECK(tallyOf(main.children[0].children[1], program, preview) == Tally::None);
	CHECK(plateau && tallyOf(plateau->children[0], program, preview) == Tally::Preview);
	CHECK(tallyOf(tree[1], program, preview) == Tally::None);
}

TEST("nav: folded contains")
{
	CHECK(containsFolded("Problème technique", "PROBLEME"));
	CHECK(containsFolded("Éditeur", "edit"));
	CHECK(!containsFolded("Duo", "trio"));
	CHECK(containsFolded("Duo ▯", "▯"));
}

TEST("live: seen by an audience only when streamed or on a virtual camera")
{
	live::Facts f;
	f.onProgram = true;
	f.mainCanvas = true;
	CHECK(!live::seenByAudience(f)); /* program, nothing runs */
	f.outputs.push_back({true, true, false, false}); /* recording the main canvas */
	CHECK(!live::seenByAudience(f));
	f.mainStreaming = true;
	CHECK(live::seenByAudience(f));
	f.mainStreaming = false;
	f.mainVirtualCam = true;
	CHECK(live::seenByAudience(f));
	f.onProgram = false; /* streaming, but another scene is on the program */
	CHECK(!live::seenByAudience(f));
}

TEST("live: other canvases need an active stream or virtual camera of their own")
{
	live::Facts f;
	f.onProgram = true;
	f.mainCanvas = false;
	f.mainStreaming = true; /* the 16:9 stream does not show the vertical canvas */
	f.mainVirtualCam = true;
	CHECK(!live::seenByAudience(f));
	f.outputs.push_back({true, false, true, false}); /* a stream of another canvas */
	CHECK(!live::seenByAudience(f));
	f.outputs.push_back({false, true, true, false}); /* stream of this canvas, stopped */
	CHECK(!live::seenByAudience(f));
	f.outputs.push_back({true, true, false, false}); /* recording of this canvas */
	CHECK(!live::seenByAudience(f));
	f.outputs.push_back({true, true, false, true}); /* Vertical's virtual camera */
	CHECK(live::seenByAudience(f));
	f.outputs.back() = {true, true, true, false}; /* Vertical's stream */
	CHECK(live::seenByAudience(f));
	f.onProgram = false;
	CHECK(!live::seenByAudience(f));
}
