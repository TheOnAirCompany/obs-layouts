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

#include "guides.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace layouts::core {

namespace {

const std::vector<Guide> kEmpty;

bool validPos(double p)
{
	return std::isfinite(p) && std::fabs(p) <= kGuideLimit;
}

} // namespace

const std::vector<Guide> &GuideDocument::sceneGuides(const std::string &uuid) const
{
	auto it = scenes_.find(uuid);
	return it == scenes_.end() ? kEmpty : it->second.guides;
}

std::vector<Guide> &GuideDocument::sceneGuidesMutable(const std::string &uuid, const std::string &name)
{
	SceneGuides &s = scenes_[uuid];
	if (!name.empty())
		s.name = name;
	return s.guides;
}

std::vector<GuideDocument::Ref> GuideDocument::visibleGuides(const std::string &uuid) const
{
	std::vector<Ref> out;
	for (const Guide &g : sceneGuides(uuid))
		out.push_back({false, g});
	for (const Guide &g : global_)
		out.push_back({true, g});
	return out;
}

uint64_t GuideDocument::add(const std::string &uuid, const std::string &name, Axis axis, double pos, bool global)
{
	if (!validPos(pos))
		return 0;
	Guide g;
	g.id = nextId_++;
	g.axis = axis;
	g.pos = pos;
	if (global)
		global_.push_back(g);
	else
		sceneGuidesMutable(uuid, name).push_back(g);
	return g.id;
}

static Guide *findIn(std::vector<Guide> &v, uint64_t id)
{
	auto it = std::find_if(v.begin(), v.end(), [id](const Guide &g) { return g.id == id; });
	return it == v.end() ? nullptr : &*it;
}

bool GuideDocument::move(uint64_t id, double pos)
{
	if (!validPos(pos))
		return false;
	if (Guide *g = findIn(global_, id)) {
		g->pos = pos;
		return true;
	}
	for (auto &[uuid, s] : scenes_) {
		if (Guide *g = findIn(s.guides, id)) {
			g->pos = pos;
			return true;
		}
	}
	return false;
}

bool GuideDocument::setLocked(uint64_t id, bool locked)
{
	if (Guide *g = findIn(global_, id)) {
		g->locked = locked;
		return true;
	}
	for (auto &[uuid, s] : scenes_) {
		if (Guide *g = findIn(s.guides, id)) {
			g->locked = locked;
			return true;
		}
	}
	return false;
}

bool GuideDocument::remove(uint64_t id)
{
	auto erase = [id](std::vector<Guide> &v) {
		const size_t n = v.size();
		v.erase(std::remove_if(v.begin(), v.end(), [id](const Guide &g) { return g.id == id; }), v.end());
		return v.size() != n;
	};
	if (erase(global_))
		return true;
	for (auto &[uuid, s] : scenes_) {
		if (erase(s.guides))
			return true;
	}
	return false;
}

std::optional<GuideDocument::Ref> GuideDocument::find(uint64_t id) const
{
	for (const Guide &g : global_) {
		if (g.id == id)
			return Ref{true, g};
	}
	for (const auto &[uuid, s] : scenes_) {
		for (const Guide &g : s.guides) {
			if (g.id == id)
				return Ref{false, g};
		}
	}
	return std::nullopt;
}

bool GuideDocument::setGlobal(uint64_t id, const std::string &uuid, const std::string &name, bool global)
{
	const std::optional<Ref> r = find(id);
	if (!r || r->global == global)
		return false;
	remove(id);
	if (global)
		global_.push_back(r->guide);
	else
		sceneGuidesMutable(uuid, name).push_back(r->guide);
	return true;
}

void GuideDocument::clearScene(const std::string &uuid)
{
	scenes_.erase(uuid);
}

void GuideDocument::clearAll()
{
	scenes_.clear();
	global_.clear();
}

void GuideDocument::clear()
{
	clearAll();
	nextId_ = 1;
}

void GuideDocument::adoptByName(const std::string &uuid, const std::string &name)
{
	if (uuid.empty() || name.empty() || scenes_.count(uuid))
		return;
	for (auto it = scenes_.begin(); it != scenes_.end(); ++it) {
		if (it->second.name == name) {
			SceneGuides moved = std::move(it->second);
			scenes_.erase(it);
			scenes_[uuid] = std::move(moved);
			return;
		}
	}
}

void GuideDocument::pruneOrphans(const std::vector<std::string> &alive)
{
	const std::set<std::string> keep(alive.begin(), alive.end());
	for (auto it = scenes_.begin(); it != scenes_.end();) {
		if (!keep.count(it->first) || it->second.guides.empty())
			it = scenes_.erase(it);
		else
			++it;
	}
}

GuideDocument::GuideRecord GuideDocument::toRecord(const Guide &g)
{
	return {g.axis == Axis::X ? "x" : "y", g.pos, g.locked};
}

std::optional<Guide> GuideDocument::fromRecord(const GuideRecord &r, int version)
{
	Guide g;
	if (version <= 0) {
		/* v0 (development builds): "v" = vertical line, "h" = horizontal line. */
		if (r.axis == "v")
			g.axis = Axis::X;
		else if (r.axis == "h")
			g.axis = Axis::Y;
		else
			return std::nullopt;
	} else {
		if (r.axis == "x")
			g.axis = Axis::X;
		else if (r.axis == "y")
			g.axis = Axis::Y;
		else
			return std::nullopt;
	}
	if (!validPos(r.pos))
		return std::nullopt;
	g.pos = r.pos;
	g.locked = r.locked;
	g.id = nextId_++;
	return g;
}

GuideDocument::Records GuideDocument::toRecords() const
{
	Records r;
	r.version = kGuidesSchemaVersion;
	for (const auto &[uuid, s] : scenes_) {
		if (s.guides.empty())
			continue;
		SceneRecord sr;
		sr.uuid = uuid;
		sr.name = s.name;
		for (const Guide &g : s.guides)
			sr.guides.push_back(toRecord(g));
		r.scenes.push_back(std::move(sr));
	}
	for (const Guide &g : global_)
		r.global.push_back(toRecord(g));
	return r;
}

int GuideDocument::fromRecords(const Records &r)
{
	clear();
	int rejected = 0;
	if (r.version > kGuidesSchemaVersion) {
		/* Written by a newer Layouts: read what we understand, the axis/pos fields are
		 * stable across versions by design. */
	}
	const int v = std::min(r.version, kGuidesSchemaVersion);
	for (const SceneRecord &sr : r.scenes) {
		if (sr.uuid.empty() && sr.name.empty()) {
			rejected += (int)sr.guides.size();
			continue;
		}
		const std::string key = sr.uuid.empty() ? ("name:" + sr.name) : sr.uuid;
		for (const GuideRecord &gr : sr.guides) {
			if (std::optional<Guide> g = fromRecord(gr, v))
				sceneGuidesMutable(key, sr.name).push_back(*g);
			else
				rejected++;
		}
	}
	for (const GuideRecord &gr : r.global) {
		if (std::optional<Guide> g = fromRecord(gr, v))
			global_.push_back(*g);
		else
			rejected++;
	}
	return rejected;
}

} // namespace layouts::core
