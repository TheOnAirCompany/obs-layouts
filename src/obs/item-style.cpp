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

#include "item-style.hpp"

#include "edit-context.hpp"
#include "roles.hpp"
#include "store.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>

#include <QString>

#include "corners.hpp"
#include "style-render.hpp"

#include <cstring>
#include <string>

namespace layouts {

namespace itemstyle {

namespace {

QString tr(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

std::string readOverride(obs_sceneitem_t *item)
{
	OBSDataAutoRelease priv = obs_sceneitem_get_private_settings(item);
	const char *json = priv ? obs_data_get_string(priv, kSettingKey) : nullptr;
	return json ? json : "";
}

void writeOverride(obs_sceneitem_t *item, const std::string &json)
{
	OBSDataAutoRelease priv = obs_sceneitem_get_private_settings(item);
	if (!priv)
		return;
	if (json.empty())
		obs_data_erase(priv, kSettingKey);
	else
		obs_data_set_string(priv, kSettingKey, json.c_str());
}

/* Undo record: {items: [{scene, id, style}]}, scene = uuid of the scene or group. */
obs_data_t *snapshot(const std::vector<obs_sceneitem_t *> &items)
{
	obs_data_t *d = obs_data_create();
	OBSDataArrayAutoRelease arr = obs_data_array_create();
	for (obs_sceneitem_t *item : items) {
		obs_scene_t *scene = obs_sceneitem_get_scene(item);
		if (!scene)
			continue;
		OBSDataAutoRelease it = obs_data_create();
		obs_data_set_string(it, "scene", obs_source_get_uuid(obs_scene_get_source(scene)));
		obs_data_set_int(it, "id", obs_sceneitem_get_id(item));
		obs_data_set_string(it, "style", readOverride(item).c_str());
		obs_data_array_push_back(arr, it);
	}
	obs_data_set_array(d, "items", arr);
	return d;
}

void applyData(const char *json)
{
	OBSDataAutoRelease d = obs_data_create_from_json(json);
	OBSDataArrayAutoRelease arr = d ? obs_data_get_array(d, "items") : nullptr;
	const size_t n = arr ? obs_data_array_count(arr) : 0;
	for (size_t i = 0; i < n; i++) {
		OBSDataAutoRelease it = obs_data_array_item(arr, i);
		OBSSourceAutoRelease src = obs_get_source_by_uuid(obs_data_get_string(it, "scene"));
		obs_scene_t *scene = src ? obs_group_or_scene_from_source(src) : nullptr;
		obs_sceneitem_t *item = scene ? obs_scene_find_sceneitem_by_id(scene, obs_data_get_int(it, "id"))
					      : nullptr;
		if (!item)
			continue;
		writeOverride(item, obs_data_get_string(it, "style"));
		render(item);
	}
	EditContext::instance().markDirty();
}

void addUndo(obs_data_t *before, obs_data_t *after)
{
	if (strcmp(obs_data_get_json(before), obs_data_get_json(after)) == 0)
		return;
	const QString name = tr("Undo.Format").arg(tr("Undo.Style"), EditContext::instance().sceneName());
	obs_frontend_add_undo_redo_action(name.toUtf8().constData(), applyData, applyData, obs_data_get_json(before),
					  obs_data_get_json(after), false);
}

} // namespace

core::Role itemRole(obs_sceneitem_t *item)
{
	obs_source_t *src = obs_sceneitem_get_source(item);
	return src ? roles::role(src) : core::Role::None;
}

bool hasOverride(obs_sceneitem_t *item)
{
	return !readOverride(item).empty();
}

core::Style typeDefault(obs_sceneitem_t *item)
{
	return Store::instance().typeStyle(itemRole(item));
}

core::Style effective(obs_sceneitem_t *item)
{
	const std::string json = readOverride(item);
	core::Style s;
	if (!json.empty() && core::fromJson(json, s))
		return s;
	return typeDefault(item);
}

bool sameStyle(const core::Style &a, const core::Style &b)
{
	return core::toJson(a) == core::toJson(b);
}

bool differsFromType(obs_sceneitem_t *item)
{
	return hasOverride(item) && !sameStyle(effective(item), typeDefault(item));
}

void edit(const std::vector<obs_sceneitem_t *> &items, const std::function<void(core::Style &)> &change)
{
	if (items.empty())
		return;
	OBSDataAutoRelease before = snapshot(items);
	for (obs_sceneitem_t *item : items) {
		core::Style s = effective(item);
		change(s);
		writeOverride(item, core::toJson(s));
		render(item);
	}
	OBSDataAutoRelease after = snapshot(items);
	addUndo(before, after);
	EditContext::instance().markDirty();
}

void resetToType(const std::vector<obs_sceneitem_t *> &items)
{
	if (items.empty())
		return;
	OBSDataAutoRelease before = snapshot(items);
	for (obs_sceneitem_t *item : items) {
		writeOverride(item, "");
		render(item);
	}
	OBSDataAutoRelease after = snapshot(items);
	addUndo(before, after);
	EditContext::instance().markDirty();
}

void render(obs_sceneitem_t *item)
{
	const core::Style s = effective(item);
	corners::applyItemStyle(item, s, nullptr, false);
	obs::style::setBorderShadow(item, s, false);
}

void reapplyType(core::Role role)
{
	obs_enum_scenes(
		[](void *p, obs_source_t *sceneSource) {
			obs_scene_t *scene = obs_group_or_scene_from_source(sceneSource);
			if (scene)
				obs_scene_enum_items(
					scene,
					[](obs_scene_t *, obs_sceneitem_t *item, void *p) {
						const core::Role r = *static_cast<core::Role *>(p);
						if (!obs_sceneitem_is_group(item) && !hasOverride(item) &&
						    itemRole(item) == r)
							render(item);
						return true;
					},
					p);
			return true;
		},
		&role);
	EditContext::instance().markDirty();
}

} // namespace itemstyle
} // namespace layouts
