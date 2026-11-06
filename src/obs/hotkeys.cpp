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

#include "hotkeys.hpp"

#include "commands.hpp"
#include "store.hpp"

#include <obs-module.h>
#include <obs.hpp>
#include <plugin-support.h>
#include <util/platform.h>

#include <QCoreApplication>
#include <QMetaObject>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace layouts::hotkeys {

namespace {

struct Action {
	std::string name;
	std::string descriptionKey;
	std::function<void()> run;
	obs_hotkey_id id = OBS_INVALID_HOTKEY_ID;
};

std::vector<std::unique_ptr<Action>> g_actions;

void onHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	auto *a = static_cast<Action *>(data);
	/* Hotkeys fire on the hotkey thread: run the action on the UI thread. */
	QMetaObject::invokeMethod(QCoreApplication::instance(), [a] { a->run(); }, Qt::QueuedConnection);
}

std::string path()
{
	char *p = obs_module_config_path("hotkeys.json");
	std::string s = p ? p : "";
	bfree(p);
	return s;
}

std::function<void()> toggle(bool ViewSettings::*field)
{
	return [field] {
		Store::instance().settings().*field = !(Store::instance().settings().*field);
		Store::instance().notifySettingsChanged();
	};
}

} // namespace

void registerAll(std::function<void()> zoomFit, std::function<void()> openWindow, std::function<void()> openBuilder)
{
	if (!g_actions.empty())
		return;
	auto add = [](const char *name, const char *key, std::function<void()> fn) {
		auto a = std::make_unique<Action>();
		a->name = name;
		a->descriptionKey = key;
		a->run = std::move(fn);
		g_actions.push_back(std::move(a));
	};
	/* The 0.6 "open the Layouts window" hotkey keeps its name: it opens the Scene Editor. */
	add("layouts.open_window", "Hotkey.OpenWindow", std::move(openWindow));
	add("layouts.open_builder", "Hotkey.OpenBuilder", std::move(openBuilder));
	add("layouts.toggle_rulers", "Hotkey.Rulers", toggle(&ViewSettings::rulers));
	add("layouts.toggle_guides", "Hotkey.Guides", toggle(&ViewSettings::guides));
	add("layouts.toggle_grid", "Hotkey.Grid", toggle(&ViewSettings::grid));
	add("layouts.toggle_safe_areas", "Hotkey.SafeAreas", toggle(&ViewSettings::safeAreas));
	add("layouts.toggle_snapping", "Hotkey.Snapping", [] {
		Store::instance().settings().snap.enabled = !Store::instance().settings().snap.enabled;
		Store::instance().notifySettingsChanged();
	});
	add("layouts.align_left", "Hotkey.AlignLeft", [] { commands::align(core::AlignEdge::Left); });
	add("layouts.align_hcenter", "Hotkey.AlignHCenter", [] { commands::align(core::AlignEdge::HCenter); });
	add("layouts.align_right", "Hotkey.AlignRight", [] { commands::align(core::AlignEdge::Right); });
	add("layouts.align_top", "Hotkey.AlignTop", [] { commands::align(core::AlignEdge::Top); });
	add("layouts.align_vcenter", "Hotkey.AlignVCenter", [] { commands::align(core::AlignEdge::VCenter); });
	add("layouts.align_bottom", "Hotkey.AlignBottom", [] { commands::align(core::AlignEdge::Bottom); });
	add("layouts.distribute_h", "Hotkey.DistributeH", [] { commands::distribute(core::Axis::X, true); });
	add("layouts.distribute_v", "Hotkey.DistributeV", [] { commands::distribute(core::Axis::Y, true); });
	add("layouts.zoom_fit", "Hotkey.ZoomFit", std::move(zoomFit));

	OBSDataAutoRelease saved = obs_data_create_from_json_file_safe(path().c_str(), "bak");
	for (auto &a : g_actions) {
		const std::string desc = std::string("Layouts: ") + obs_module_text(a->descriptionKey.c_str());
		a->id = obs_hotkey_register_frontend(a->name.c_str(), desc.c_str(), onHotkey, a.get());
		if (saved) {
			OBSDataArrayAutoRelease arr = obs_data_get_array(saved, a->name.c_str());
			if (arr)
				obs_hotkey_load(a->id, arr);
		}
	}
}

void save()
{
	if (g_actions.empty())
		return;
	OBSDataAutoRelease d = obs_data_create();
	for (auto &a : g_actions) {
		OBSDataArrayAutoRelease arr = obs_hotkey_save(a->id);
		obs_data_set_array(d, a->name.c_str(), arr);
	}
	char *dir = obs_module_config_path("");
	if (dir) {
		os_mkdirs(dir);
		bfree(dir);
	}
	if (!obs_data_save_json_safe(d, path().c_str(), "tmp", "bak"))
		obs_log(LOG_WARNING, "could not save hotkeys");
}

void unregisterAll()
{
	for (auto &a : g_actions) {
		if (a->id != OBS_INVALID_HOTKEY_ID)
			obs_hotkey_unregister(a->id);
	}
	g_actions.clear();
}

bool trigger(const char *name)
{
	for (auto &a : g_actions) {
		if (a->name == name) {
			a->run();
			return true;
		}
	}
	return false;
}

} // namespace layouts::hotkeys
