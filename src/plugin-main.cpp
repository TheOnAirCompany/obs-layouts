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

#include <QDockWidget>
#include <QMainWindow>
#include <QPointer>
#include <QTimer>

#include "obs/constraints.hpp"
#include "obs/edit-context.hpp"
#include "obs/hotkeys.hpp"
#include "obs/store.hpp"
#include "ui/canvas-dock.hpp"
#include "ui/canvas-view.hpp"
#include "ui/inspector-dock.hpp"
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
void runSelfTest(ui::CanvasDock *canvas);
}
#endif

namespace {

constexpr const char *kInspectorId = "layouts-inspector";
constexpr const char *kCanvasId = "layouts-canvas";

QPointer<layouts::ui::InspectorDock> g_inspector;
QPointer<layouts::ui::CanvasDock> g_canvas;

void placeDocksOnFirstRun()
{
	layouts::ViewSettings &s = layouts::Store::instance().settings();
	if (s.firstRunDone)
		return;
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (g_inspector) {
		if (auto *d = qobject_cast<QDockWidget *>(g_inspector->parentWidget())) {
			d->setFloating(false);
			main->addDockWidget(Qt::RightDockWidgetArea, d);
			d->show();
			main->resizeDocks({d}, {layouts::tokens::kInspectorWidth}, Qt::Horizontal);
		}
	}
	if (g_canvas) {
		if (auto *d = qobject_cast<QDockWidget *>(g_canvas->parentWidget())) {
			d->setFloating(true);
			d->resize(1100, 700);
			d->show();
		}
	}
	s.firstRunDone = true;
	layouts::Store::instance().saveSettings();
}

void onFrontendEvent(enum obs_frontend_event event, void *)
{
	layouts::EditContext::instance().handleFrontendEvent(event);
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
	}
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		layouts::ConstraintWatcher::instance().start();
		placeDocksOnFirstRun();
#ifdef LAYOUTS_SELFTEST
		if (getenv("LAYOUTS_SELFTEST"))
			QTimer::singleShot(1500, [] { layouts::runSelfTest(g_canvas); });
#endif
	}
}

void onSave(obs_data_t *data, bool saving, void *)
{
	if (saving) {
		layouts::Store::instance().saveGuides(data);
		layouts::hotkeys::save(); /* bindings may have changed in Settings > Hotkeys */
	} else
		layouts::Store::instance().loadGuides(data);
}

} // namespace

bool obs_module_load(void)
{
	Q_INIT_RESOURCE(layouts);

	layouts::Store::instance().loadSettings();
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

	g_inspector = new layouts::ui::InspectorDock(main);
	if (!obs_frontend_add_dock_by_id(kInspectorId, obs_module_text("Inspector.Title"), g_inspector.data())) {
		obs_log(LOG_ERROR, "could not register the Inspector dock");
		delete g_inspector.data();
		return false;
	}
	g_canvas = new layouts::ui::CanvasDock(main);
	if (!obs_frontend_add_dock_by_id(kCanvasId, obs_module_text("Canvas.Title"), g_canvas.data())) {
		obs_log(LOG_ERROR, "could not register the Canvas dock");
		delete g_canvas.data();
		return false;
	}

	layouts::hotkeys::registerAll([] {
		if (g_canvas)
			g_canvas->view()->zoomFit();
	});

	obs_frontend_add_event_callback(onFrontendEvent, nullptr);
	obs_frontend_add_save_callback(onSave, nullptr);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_save_callback(onSave, nullptr);
	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
	layouts::ConstraintWatcher::instance().stop();
	layouts::EditContext::instance().shutdown();
	obs_log(LOG_INFO, "plugin unloaded");
}
