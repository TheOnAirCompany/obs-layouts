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

/* Guides and their persistent document.
 *
 * Guides live either on one scene (keyed by the scene UUID, with the scene name kept
 * as a fallback for collections imported from another machine) or globally (shown on
 * every scene of the scene collection). The document is stored in the scene
 * collection through obs_frontend_add_save_callback(); the integration layer maps it
 * to obs_data. Schema history is documented in docs/ARCHITECTURE.md. */

#include "geometry.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace layouts::core {

constexpr int kGuidesSchemaVersion = 1;
constexpr double kGuideLimit = 100000.0; /* |position| beyond this is rejected */

struct Guide {
	uint64_t id = 0;
	Axis axis = Axis::X; /* X: vertical line at x = pos; Y: horizontal line at y = pos */
	double pos = 0.0;
	bool locked = false;
};

struct SceneGuides {
	std::string name; /* fallback key */
	std::vector<Guide> guides;
};

class GuideDocument {
public:
	/* ---- per scene ---- */
	const std::vector<Guide> &sceneGuides(const std::string &uuid) const;
	std::vector<Guide> &sceneGuidesMutable(const std::string &uuid, const std::string &name);

	/* ---- global ---- */
	const std::vector<Guide> &globalGuides() const { return global_; }
	std::vector<Guide> &globalGuidesMutable() { return global_; }

	/* Guides visible on a scene: scene guides followed by global ones. */
	struct Ref {
		bool global;
		Guide guide;
	};
	std::vector<Ref> visibleGuides(const std::string &uuid) const;

	uint64_t add(const std::string &uuid, const std::string &name, Axis axis, double pos, bool global);
	bool move(uint64_t id, double pos);
	bool setLocked(uint64_t id, bool locked);
	bool remove(uint64_t id);
	/* Moves a guide between its scene list and the global list. */
	bool setGlobal(uint64_t id, const std::string &uuid, const std::string &name, bool global);
	std::optional<Ref> find(uint64_t id) const;

	void clearScene(const std::string &uuid);
	void clearAll();

	/* Scene that was renamed / re-created with another UUID but the same name. */
	void adoptByName(const std::string &uuid, const std::string &name);

	/* Keeps only scenes in `alive` (orphans are dropped when a collection is saved;
	 * an orphan survives as long as the scene exists under any of the given keys). */
	void pruneOrphans(const std::vector<std::string> &aliveUuids);

	const std::map<std::string, SceneGuides> &scenes() const { return scenes_; }
	void clear();

	/* ---- persistence records (schema-independent plain data) ---- */
	struct GuideRecord {
		std::string axis; /* "x" | "y" (v0: "v" | "h") */
		double pos = 0.0;
		bool locked = false;
	};
	struct SceneRecord {
		std::string uuid;
		std::string name;
		std::vector<GuideRecord> guides;
	};
	struct Records {
		int version = kGuidesSchemaVersion;
		std::vector<SceneRecord> scenes;
		std::vector<GuideRecord> global;
	};

	Records toRecords() const;
	/* Accepts any known schema version, drops invalid entries, never throws.
	 * Returns the number of rejected entries. */
	int fromRecords(const Records &r);

private:
	std::map<std::string, SceneGuides> scenes_;
	std::vector<Guide> global_;
	uint64_t nextId_ = 1;

	std::optional<Guide> fromRecord(const GuideRecord &r, int version);
	static GuideRecord toRecord(const Guide &g);
};

} // namespace layouts::core
