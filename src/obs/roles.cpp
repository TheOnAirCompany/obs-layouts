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

#include "roles.hpp"

#include "edit-context.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QString>

#include <cstring>

namespace layouts::roles {

namespace {

constexpr const char *kRoleKey = "layouts_role";
constexpr const char *kMainKey = "layouts_main";

QString tr(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

bool idStartsWith(obs_source_t *source, std::initializer_list<const char *> ids)
{
	const char *id = source ? obs_source_get_unversioned_id(source) : nullptr;
	if (!id)
		return false;
	for (const char *c : ids) {
		if (strncmp(id, c, strlen(c)) == 0)
			return true;
	}
	return false;
}

void writeRole(obs_source_t *source, core::Role r)
{
	OBSDataAutoRelease priv = obs_source_get_private_settings(source);
	if (r == core::Role::None)
		obs_data_erase(priv, kRoleKey);
	else
		obs_data_set_string(priv, kRoleKey, core::roleKey(r));
	if (r != core::Role::Camera)
		obs_data_erase(priv, kMainKey);
}

/* Undo data: {items: [{source, role}]} with "" for "never set". */
void applyData(const char *json)
{
	OBSDataAutoRelease d = obs_data_create_from_json(json);
	OBSDataArrayAutoRelease items = obs_data_get_array(d, "items");
	for (size_t i = 0; i < obs_data_array_count(items); i++) {
		OBSDataAutoRelease it = obs_data_array_item(items, i);
		OBSSourceAutoRelease source = obs_get_source_by_uuid(obs_data_get_string(it, "source"));
		if (!source)
			continue;
		writeRole(source, core::roleFromKey(obs_data_get_string(it, "role")));
		if (obs_data_get_bool(it, "main")) {
			OBSDataAutoRelease priv = obs_source_get_private_settings(source);
			obs_data_set_bool(priv, kMainKey, true);
		}
	}
	EditContext::instance().markDirty();
}

obs_data_t *snapshot(const std::vector<obs_source_t *> &sources)
{
	obs_data_t *d = obs_data_create();
	OBSDataArrayAutoRelease items = obs_data_array_create();
	for (obs_source_t *s : sources) {
		OBSDataAutoRelease it = obs_data_create();
		obs_data_set_string(it, "source", obs_source_get_uuid(s));
		obs_data_set_string(it, "role", core::roleKey(explicitRole(s)));
		obs_data_set_bool(it, "main", isMain(s));
		obs_data_array_push_back(items, it);
	}
	obs_data_set_array(d, "items", items);
	return d;
}

} // namespace

bool isCameraKind(obs_source_t *source)
{
	return idStartsWith(source, {"av_capture_input", "macos-avcapture", "dshow_input", "v4l2_input",
				     "decklink-input", "ndi_source", "aja_source"});
}

bool isScreenKind(obs_source_t *source)
{
	return idStartsWith(source,
			    {"display_capture", "monitor_capture", "window_capture", "screen_capture", "game_capture",
			     "xshm_input", "xcomposite_input", "pipewire-desktop-capture-source",
			     "pipewire-window-capture-source", "pipewire-screen-capture-source", "syphon-input"});
}

core::Role explicitRole(obs_source_t *source)
{
	if (!source)
		return core::Role::None;
	OBSDataAutoRelease priv = obs_source_get_private_settings(source);
	return core::roleFromKey(obs_data_get_string(priv, kRoleKey));
}

core::Role suggestedRole(obs_source_t *source)
{
	if (!source)
		return core::Role::None;
	const core::Role byName = core::roleFromName(obs_source_get_name(source));
	if (byName != core::Role::None)
		return byName;
	if (isCameraKind(source))
		return core::Role::Camera;
	if (isScreenKind(source))
		return core::Role::Screen;
	return core::Role::None;
}

core::Role role(obs_source_t *source)
{
	const core::Role r = explicitRole(source);
	return r != core::Role::None ? r : suggestedRole(source);
}

bool isMain(obs_source_t *source)
{
	if (!source)
		return false;
	OBSDataAutoRelease priv = obs_source_get_private_settings(source);
	return obs_data_get_bool(priv, kMainKey);
}

void setRole(const std::vector<obs_source_t *> &sources, core::Role r)
{
	if (sources.empty())
		return;
	OBSDataAutoRelease before = snapshot(sources);
	for (obs_source_t *s : sources)
		writeRole(s, r);
	OBSDataAutoRelease after = snapshot(sources);
	if (strcmp(obs_data_get_json(before), obs_data_get_json(after)) != 0) {
		const QString name = tr("Undo.Format").arg(tr("Undo.Role"), EditContext::instance().sceneName());
		obs_frontend_add_undo_redo_action(name.toUtf8().constData(), applyData, applyData,
						  obs_data_get_json(before), obs_data_get_json(after), false);
	}
	EditContext::instance().markDirty();
}

void setMain(obs_source_t *source, bool main)
{
	if (!source)
		return;
	if (main) {
		obs_enum_sources(
			[](void *, obs_source_t *s) {
				OBSDataAutoRelease priv = obs_source_get_private_settings(s);
				obs_data_erase(priv, kMainKey);
				return true;
			},
			nullptr);
		if (explicitRole(source) != core::Role::Camera)
			writeRole(source, core::Role::Camera);
	}
	OBSDataAutoRelease priv = obs_source_get_private_settings(source);
	obs_data_set_bool(priv, kMainKey, main);
	EditContext::instance().markDirty();
}

} // namespace layouts::roles
