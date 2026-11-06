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

#include "scene-nav.hpp"

#include "json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <numeric>

namespace layouts::core::nav {

namespace {

void readNodes(const JsonValue &arr, std::vector<OrgNode> &out)
{
	for (const JsonValue &v : arr.items) {
		if (v.type != JsonValue::Object)
			continue;
		const std::string type = v.string("type");
		OrgNode n;
		if (type == "stack") {
			n.kind = OrgNode::Kind::Stack;
			n.name = v.string("name");
			n.color = v.string("color");
			n.collapsed = v.boolean("collapsed", false);
			if (const JsonValue *c = v.get("children"); c && c->type == JsonValue::Array)
				readNodes(*c, n.children);
		} else if (type == "scene") {
			if (v.get("missing_since"))
				continue; /* a deleted scene Stacks keeps for undo */
			n.kind = OrgNode::Kind::Scene;
			n.uuid = v.string("uuid");
			n.name = v.string("name");
			n.favorite = v.boolean("favorite", false);
		} else {
			continue; /* separators and unknown nodes */
		}
		out.push_back(std::move(n));
	}
}

/* Folds one UTF-8 character starting at s[i] to lowercase ASCII when it is a letter of
 * Latin-1 (accents dropped); other characters are copied. */
std::string fold(const std::string &s)
{
	static const char *const kLatin1 = /* U+00C0 .. U+00FF */
		"aaaaaaaceeeeiiii"
		"dnooooo*ouuuuyts"
		"aaaaaaaceeeeiiii"
		"dnooooo/ouuuuyty";
	std::string out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); i++) {
		const unsigned char c = (unsigned char)s[i];
		if (c < 0x80) {
			out += (char)((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
			continue;
		}
		if ((c == 0xC3) && i + 1 < s.size()) {
			const unsigned char d = (unsigned char)s[i + 1];
			if (d >= 0x80 && d <= 0xBF) {
				out += kLatin1[d - 0x80];
				i++;
				continue;
			}
		}
		out += (char)c;
	}
	return out;
}

bool passes(const Node &scene, const Filter &f, const std::string &query)
{
	if (f.ratio == RatioFilter::Favorites && !scene.favorite)
		return false;
	return query.empty() || fold(scene.name).find(query) != std::string::npos;
}

bool filterInto(const Node &node, const Filter &f, const std::string &query, bool dropEmpty, Node &out)
{
	if (node.kind == Node::Kind::Scene) {
		if (!passes(node, f, query))
			return false;
		out = node;
		return true;
	}
	out = node;
	out.children.clear();
	for (const Node &c : node.children) {
		Node kept;
		if (filterInto(c, f, query, dropEmpty, kept))
			out.children.push_back(std::move(kept));
	}
	return !dropEmpty || sceneCount(out) > 0;
}

void collectPaths(const std::vector<Node> &nodes, std::vector<std::string> &out)
{
	for (const Node &n : nodes) {
		if (n.kind != Node::Kind::Stack)
			continue;
		out.push_back(n.path);
		collectPaths(n.children, out);
	}
}

bool anyIn(const Node &n, const std::set<std::string> &uuids)
{
	if (n.kind == Node::Kind::Scene)
		return uuids.count(n.uuid) > 0;
	for (const Node &c : n.children) {
		if (anyIn(c, uuids))
			return true;
	}
	return false;
}

} // namespace

bool containsFolded(const std::string &text, const std::string &query)
{
	return fold(text).find(fold(query)) != std::string::npos;
}

bool parseOrganization(const std::string &json, std::vector<OrgNode> &out)
{
	out.clear();
	JsonValue root;
	if (json.empty() || !parseJson(json, root) || root.type != JsonValue::Object)
		return false;
	if (root.number("schema_version", 1) > 1)
		return false;
	const JsonValue *tree = root.get("tree");
	if (!tree || tree->type != JsonValue::Array)
		return false;
	readNodes(*tree, out);
	return true;
}

Ratio ratioOf(uint32_t width, uint32_t height)
{
	if (width == height)
		return Ratio::Square;
	return width > height ? Ratio::Landscape : Ratio::Portrait;
}

std::string ratioLabel(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0)
		return {};
	static const int kKnown[][2] = {{16, 9}, {9, 16}, {4, 3},   {3, 4},   {1, 1},
					{4, 5},  {5, 4},  {16, 10}, {10, 16}, {21, 9}};
	const double r = (double)width / (double)height;
	for (const auto &k : kKnown) {
		const double kr = (double)k[0] / (double)k[1];
		if (std::fabs(r - kr) / kr < 0.01)
			return std::to_string(k[0]) + ":" + std::to_string(k[1]);
	}
	const uint32_t g = std::gcd(width, height);
	const uint32_t a = width / g, b = height / g;
	if (a <= 50 && b <= 50)
		return std::to_string(a) + ":" + std::to_string(b);
	char buf[32];
	if (r >= 1)
		std::snprintf(buf, sizeof(buf), "%.2f:1", r);
	else
		std::snprintf(buf, sizeof(buf), "1:%.2f", 1.0 / r);
	return buf;
}

std::vector<Node> buildTree(const std::vector<CanvasInfo> &canvases, const std::vector<OrgNode> *org)
{
	std::vector<const CanvasInfo *> order;
	for (const CanvasInfo &c : canvases)
		order.push_back(&c);
	std::stable_sort(order.begin(), order.end(),
			 [](const CanvasInfo *a, const CanvasInfo *b) { return a->main && !b->main; });

	std::vector<Node> out;
	for (const CanvasInfo *c : order) {
		Node section;
		section.kind = Node::Kind::Canvas;
		section.name = c->name;
		section.uuid = c->uuid;
		section.canvas = c->uuid;
		section.main = c->main;
		section.ratio = ratioLabel(c->width, c->height);
		section.orientation = ratioOf(c->width, c->height);

		std::map<std::string, size_t> byUuid, byName;
		for (size_t i = 0; i < c->scenes.size(); i++) {
			byUuid.emplace(c->scenes[i].uuid, i);
			byName.emplace(c->scenes[i].name, i);
		}
		std::vector<bool> placed(c->scenes.size(), false);
		auto sceneNode = [&](size_t i, bool favorite) {
			Node n;
			n.kind = Node::Kind::Scene;
			n.name = c->scenes[i].name;
			n.uuid = c->scenes[i].uuid;
			n.canvas = c->uuid;
			n.main = c->main;
			n.orientation = section.orientation;
			n.favorite = favorite;
			placed[i] = true;
			return n;
		};

		if (c->main && org) {
			std::function<void(const std::vector<OrgNode> &, Node &, const std::string &)> walk;
			walk = [&](const std::vector<OrgNode> &nodes, Node &parent, const std::string &path) {
				for (const OrgNode &o : nodes) {
					if (o.kind == OrgNode::Kind::Stack) {
						Node s;
						s.kind = Node::Kind::Stack;
						s.name = o.name;
						s.color = o.color;
						s.collapsed = o.collapsed;
						s.path = path.empty() ? o.name : path + " / " + o.name;
						s.canvas = section.canvas;
						s.main = true;
						s.orientation = section.orientation;
						walk(o.children, s, s.path);
						parent.children.push_back(std::move(s));
						continue;
					}
					/* By uuid, else by name (a duplicated collection has new uuids). */
					auto it = byUuid.find(o.uuid);
					if (it == byUuid.end() || placed[it->second]) {
						it = byName.find(o.name);
						if (it == byName.end() || placed[it->second])
							continue;
					}
					parent.children.push_back(sceneNode(it->second, o.favorite));
				}
			};
			walk(*org, section, std::string());
		}
		for (size_t i = 0; i < c->scenes.size(); i++) {
			if (!placed[i])
				section.children.push_back(sceneNode(i, false));
		}
		out.push_back(std::move(section));
	}
	return out;
}

std::vector<Node> filterTree(const std::vector<Node> &tree, const Filter &filter)
{
	if (!filter.active())
		return tree;
	const std::string query = fold(filter.query);
	const bool dropEmpty = !query.empty() || filter.ratio == RatioFilter::Favorites;
	std::vector<Node> out;
	for (const Node &section : tree) {
		if (filter.ratio == RatioFilter::Landscape && section.orientation != Ratio::Landscape)
			continue;
		if (filter.ratio == RatioFilter::Portrait && section.orientation != Ratio::Portrait)
			continue;
		Node kept;
		if (filterInto(section, filter, query, dropEmpty, kept))
			out.push_back(std::move(kept));
	}
	return out;
}

int sceneCount(const Node &node)
{
	if (node.kind == Node::Kind::Scene)
		return 1;
	int n = 0;
	for (const Node &c : node.children)
		n += sceneCount(c);
	return n;
}

std::vector<std::string> stackPaths(const std::vector<Node> &tree)
{
	std::vector<std::string> out;
	for (const Node &section : tree) {
		if (section.main)
			collectPaths(section.children, out);
	}
	return out;
}

Tally tallyOf(const Node &node, const std::set<std::string> &program, const std::set<std::string> &preview)
{
	if (anyIn(node, program))
		return Tally::Program;
	if (anyIn(node, preview))
		return Tally::Preview;
	return Tally::None;
}

} // namespace layouts::core::nav
