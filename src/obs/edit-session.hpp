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

/* Transactions on scene item transforms.
 *
 *  - Every change of a transaction is applied inside obs_scene_atomic_update(), so
 *    the output never shows a half-applied alignment.
 *  - begin() captures obs_scene_save_transform_states(); commit() records ONE OBS
 *    undo/redo entry with the before/after states (a whole drag is one entry).
 *  - cancel() restores the captured state (Escape during a drag). */

#include "core/item-model.hpp"

#include <obs.hpp>

#include <QString>

#include <string>
#include <vector>

namespace layouts {

struct ItemEntry;

struct Change {
	OBSSceneItem item;
	core::ItemTransform t;
};

class EditSession {
public:
	EditSession() = default;
	~EditSession();
	EditSession(const EditSession &) = delete;
	EditSession &operator=(const EditSession &) = delete;

	/* Returns false if editing is refused (no scene, block-on-air policy). */
	bool begin(const QString &undoName);
	bool active() const { return active_; }

	/* Applies the given transforms atomically. Can be called many times (drags). */
	void apply(const std::vector<Change> &changes);

	/* Records one undo entry if anything changed. repeatable merges consecutive
	 * entries with the same name (keyboard nudges), like OBS does. */
	void commit(bool repeatable = false);
	void cancel();

	/* One-shot helper: begin + apply + commit. */
	static bool run(const QString &undoName, const std::vector<Change> &changes, bool repeatable = false);

private:
	bool active_ = false;
	OBSScene scene_;
	QString name_;
	std::string before_;
	bool changed_ = false;
};

/* Applies a set of transforms without undo and without live checks (undo/redo). */
void applyChangesAtomically(obs_scene_t *scene, const std::vector<Change> &changes);

} // namespace layouts
