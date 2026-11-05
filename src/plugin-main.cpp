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

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <cstring>

#include <QMainWindow>
#include <QPointer>
#include <QTimer>

#include "obs/constraints.hpp"
#include "obs/corners.hpp"
#include "obs/edit-context.hpp"
#include "obs/hotkeys.hpp"
#include "obs/regie.hpp"
#include "obs/store.hpp"
#include "ui/canvas-dock.hpp"
#include "ui/canvas-view.hpp"
#include "ui/inspector-dock.hpp"
#include "ui/layouts-window.hpp"
#include "ui/regie-dock.hpp"
#include "ui/theme.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return "Layouts for OBS";
}

#ifdef LAYOUTS_SELFTEST
namespace layouts {
void runSelfTest(ui::LayoutsWindow *window);
}
#endif

namespace {

QPointer<layouts::ui::LayoutsWindow> g_window;

void openWindow(layouts::ui::LayoutsWindow::Mode mode)
{
	if (!g_window)
		return;
	g_window->setMode(mode);
	g_window->open();
}

void onFrontendEvent(enum obs_frontend_event event, void *)
{
	layouts::EditContext::instance().handleFrontendEvent(event);
	layouts::Regie::instance().handleFrontendEvent(event);
	if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP) {
		/* The old collection was saved just before. A new, empty collection does not
		 * call the load callback, so start from an empty document. */
		layouts::Store::instance().guides().clear();
		layouts::Store::instance().notifyGuidesChanged();
	}
	if (event == OBS_FRONTEND_EVENT_CANVAS_ADDED || event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED)
		layouts::ConstraintWatcher::instance().recordSizes();
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		layouts::ConstraintWatcher::instance().stop();
		layouts::hotkeys::save();
		layouts::hotkeys::unregisterAll();
		layouts::Regie::instance().saveHotkeys();
		layouts::Regie::instance().unregisterHotkeys();
	}
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		layouts::ConstraintWatcher::instance().start();
#ifdef LAYOUTS_SELFTEST
		if (getenv("LAYOUTS_SELFTEST"))
			QTimer::singleShot(1500, [] { layouts::runSelfTest(g_window); });
#endif
	}
}

void onSave(obs_data_t *data, bool saving, void *)
{
	if (saving) {
		layouts::Store::instance().saveGuides(data);
		layouts::Regie::instance().save(data);
		layouts::hotkeys::save(); /* bindings may have changed in Settings > Hotkeys */
		layouts::Regie::instance().saveHotkeys();
	} else {
		layouts::Store::instance().loadGuides(data);
		layouts::Regie::instance().load(data);
	}
}

} // namespace

bool obs_module_load(void)
{
	Q_INIT_RESOURCE(layouts);

	layouts::Store::instance().loadSettings();
	layouts::corners::registerFilter();
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());

#ifdef LAYOUTS_SELFTEST
	/* Development only: load nothing, quit after loading (leak baseline). */
	if (getenv("LAYOUTS_SELFTEST_LOAD_NOTHING")) {
		obs_frontend_add_event_callback(
			[](enum obs_frontend_event e, void *) {
				if (e != OBS_FRONTEND_EVENT_FINISHED_LOADING)
					return;
				if (strcmp(getenv("LAYOUTS_SELFTEST_LOAD_NOTHING"), "collections") == 0) {
					QTimer::singleShot(500, [] { obs_frontend_add_scene_collection("Raw A"); });
					QTimer::singleShot(2500, [] { obs_frontend_add_scene_collection("Raw B"); });
					QTimer::singleShot(4500,
							   [] { obs_frontend_set_current_scene_collection("Raw A"); });
				}
				if (strcmp(getenv("LAYOUTS_SELFTEST_LOAD_NOTHING"), "stay") == 0)
					return;
				QTimer::singleShot(7000, [] {
					static_cast<QMainWindow *>(obs_frontend_get_main_window())->close();
				});
			},
			nullptr);
		return true;
	}
#endif

	/* A regular window, not a dock: it can be resized, maximized and put in full
	 * screen freely, and never docks into the OBS window. */
	g_window = new layouts::ui::LayoutsWindow(main);
	obs_frontend_add_tools_menu_item(
		obs_module_text("Menu.Open"), [](void *) { openWindow(layouts::ui::LayoutsWindow::Mode::Scene); },
		nullptr);
	obs_frontend_add_tools_menu_item(
		obs_module_text("Menu.Builder"), [](void *) { openWindow(layouts::ui::LayoutsWindow::Mode::Builder); },
		nullptr);

	layouts::hotkeys::registerAll(
		[] {
			if (g_window)
				g_window->canvas()->view()->zoomFit();
		},
		[] {
			/* The hotkey keeps the current mode. */
			if (g_window)
				g_window->open();
		});

	/* Switcher: a dock of the OBS window (Docks menu), for live use. */
	obs_frontend_add_dock_by_id("layouts-regie", obs_module_text("Regie.DockTitle"),
				    new layouts::ui::RegieDock(main));
	layouts::Regie::instance().registerHotkeys();

	obs_frontend_add_event_callback(onFrontendEvent, nullptr);
	obs_frontend_add_save_callback(onSave, nullptr);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_post_load(void)
{
	/* obs-websocket is loaded by now: register the "layouts" vendor requests. */
	layouts::Regie::instance().registerVendor();
}

void obs_module_unload(void)
{
	layouts::Regie::instance().unregisterVendor();
	obs_frontend_remove_save_callback(onSave, nullptr);
	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
	layouts::ConstraintWatcher::instance().stop();
	layouts::EditContext::instance().shutdown();
	obs_log(LOG_INFO, "plugin unloaded");
}
