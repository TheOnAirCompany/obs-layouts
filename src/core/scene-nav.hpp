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

#pragma once

/* Scenes navigator of the Scene Editor (0.7), pure logic.
 *
 * The navigator is a tree: one section per canvas (the main one first), inside each the
 * stacks of Stacks for OBS when it is installed (it organizes the main canvas), else
 * the scenes of the canvas in their order. buildTree() makes it from what OBS lists and
 * the organization JSON published by Stacks; filterTree() applies the search field and
 * the All / 16:9 / 9:16 / favorites filter, keeping the parents of what matches. */

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace layouts::core::nav {

struct SceneInfo {
	std::string uuid;
	std::string name;
};

struct CanvasInfo {
	std::string uuid;
	std::string name;
	bool main = false;
	uint32_t width = 0;
	uint32_t height = 0;
	std::vector<SceneInfo> scenes; /* in OBS order */
};

/* A node of the organization of Stacks (separators are dropped). */
struct OrgNode {
	enum class Kind { Stack, Scene } kind = Kind::Scene;
	std::string name;
	std::string uuid;  /* scenes */
	std::string color; /* "#RRGGBB", optional */
	bool collapsed = false;
	bool favorite = false;
	std::vector<OrgNode> children;
};

/* Reads the JSON of stacks_get_organization. False when empty, unreadable or newer
 * than schema 1 (the navigator then shows flat lists). */
bool parseOrganization(const std::string &json, std::vector<OrgNode> &out);

enum class Ratio { Landscape, Portrait, Square };
Ratio ratioOf(uint32_t width, uint32_t height);
/* "16:9", "9:16", "4:3", "1:1"... (reduced fraction). */
std::string ratioLabel(uint32_t width, uint32_t height);

struct Node {
	enum class Kind { Canvas, Stack, Scene } kind = Kind::Scene;
	std::string name;
	std::string uuid;   /* scene or canvas uuid */
	std::string color;  /* stacks */
	std::string ratio;  /* canvases: "16:9" */
	std::string path;   /* stacks: "Show / Plateau" */
	std::string canvas; /* canvas uuid of every node */
	Ratio orientation = Ratio::Landscape;
	bool main = false; /* in the main canvas */
	bool collapsed = false;
	bool favorite = false;
	std::vector<Node> children;
};

/* Sections for these canvases; `org` (may be null) organizes the main canvas. Scenes the
 * organization does not file follow at the end, in OBS order; stacks keep their place
 * even when empty. */
std::vector<Node> buildTree(const std::vector<CanvasInfo> &canvases, const std::vector<OrgNode> *org);

enum class RatioFilter { All, Landscape, Portrait, Favorites };

struct Filter {
	std::string query; /* matches scene names, case-insensitive */
	RatioFilter ratio = RatioFilter::All;
	bool active() const { return !query.empty() || ratio != RatioFilter::All; }
};

/* The tree with only the scenes that pass (and their stacks and canvas). Without an
 * active filter the tree is returned as is. Canvas sections of the other ratio are
 * dropped; with a query or favorites, empty stacks and sections are dropped too. */
std::vector<Node> filterTree(const std::vector<Node> &tree, const Filter &filter);

/* Number of scenes under a node (stacks show it). */
int sceneCount(const Node &node);

/* The stack paths of the main canvas, depth first ("Plateau", "Show / Plateau"). */
std::vector<std::string> stackPaths(const std::vector<Node> &tree);

/* Tally of a row: a scene on program (red) or in preview (green). A stack shows the
 * tally of a scene it contains (program first), so a collapsed stack still tells. */
enum class Tally { None, Program, Preview };
Tally tallyOf(const Node &node, const std::set<std::string> &program, const std::set<std::string> &preview);

/* Case-insensitive "contains" on UTF-8 text (ASCII letters and the accented Latin-1
 * letters fold; "é" matches "e"). */
bool containsFolded(const std::string &text, const std::string &query);

} // namespace layouts::core::nav
