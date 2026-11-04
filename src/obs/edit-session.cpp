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

#include "edit-session.hpp"

#include "edit-context.hpp"
#include "obs-convert.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

namespace layouts {

namespace {

std::string saveStates(obs_scene_t *scene)
{
	OBSDataAutoRelease d = obs_scene_save_transform_states(scene, true);
	const char *json = d ? obs_data_get_json(d) : nullptr;
	return json ? std::string(json) : std::string();
}

void undoRedo(const char *data)
{
	/* Same mechanism as OBS's own transform undo. */
	obs_scene_load_transform_states(data);
}

struct AtomicParam {
	const std::vector<Change> *changes;
};

void applyLocked(void *param, obs_scene_t *)
{
	const auto *p = static_cast<AtomicParam *>(param);
	for (const Change &c : *p->changes) {
		if (!c.item)
			continue;
		obs_transform_info info = {};
		obs_sceneitem_crop crop = {};
		obs_sceneitem_get_info2(c.item, &info);
		obs_sceneitem_get_crop(c.item, &crop);

		obs_transform_info next = info;
		obs_sceneitem_crop nextCrop = crop;
		toObs(c.t, next, nextCrop);

		if (memcmp(&nextCrop, &crop, sizeof(crop)) != 0)
			obs_sceneitem_set_crop(c.item, &nextCrop);
		obs_sceneitem_set_info2(c.item, &next);
	}
}

} // namespace

void applyChangesAtomically(obs_scene_t *scene, const std::vector<Change> &changes)
{
	if (!scene || changes.empty())
		return;
	AtomicParam p{&changes};
	obs_scene_atomic_update(scene, applyLocked, &p);
}

EditSession::~EditSession()
{
	if (active_)
		commit();
}

bool EditSession::begin(const QString &undoName)
{
	if (active_)
		commit();
	EditContext &ctx = EditContext::instance();
	if (!ctx.scene() || !ctx.canEdit())
		return false;
	scene_ = ctx.scene();
	name_ = undoName;
	before_ = saveStates(scene_);
	changed_ = false;
	active_ = true;
	return true;
}

void EditSession::apply(const std::vector<Change> &changes)
{
	if (!active_ || changes.empty())
		return;
	applyChangesAtomically(scene_, changes);
	changed_ = true;
	/* libobs may refresh some matrices on the next video tick (crop, group children):
	 * the snapshot is rebuilt from the stored values right away. */
	EditContext::instance().markDirty();
}

void EditSession::commit(bool repeatable)
{
	if (!active_)
		return;
	active_ = false;
	if (!changed_)
		return;
	const std::string after = saveStates(scene_);
	if (after.empty() || before_.empty() || after == before_)
		return;
	const QString label = QString::fromUtf8(obs_module_text("Undo.Format"))
				      .arg(name_, QString::fromUtf8(obs_source_get_name(obs_scene_get_source(scene_))));
	obs_frontend_add_undo_redo_action(label.toUtf8().constData(), undoRedo, undoRedo, before_.c_str(),
					  after.c_str(), repeatable);
}

void EditSession::cancel()
{
	if (!active_)
		return;
	active_ = false;
	if (changed_ && !before_.empty())
		obs_scene_load_transform_states(before_.c_str());
}

bool EditSession::run(const QString &undoName, const std::vector<Change> &changes, bool repeatable)
{
	if (changes.empty())
		return false;
	EditSession s;
	if (!s.begin(undoName))
		return false;
	s.apply(changes);
	s.commit(repeatable);
	return true;
}

} // namespace layouts
