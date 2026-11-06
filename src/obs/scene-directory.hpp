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

/* The scenes of every canvas, organized like Stacks for OBS does when it is installed
 * (0.7). Feeds the Scenes navigator, the breadcrumb of the Canvas and the "Put in
 * stack" field of the Scene Builder. Also the tallies (program / preview) and the
 * links to the sister plugins, found through the global proc handler:
 *   Stacks for OBS:   stacks_get_organization, stacks_place_scene, signal stacks_changed
 *   Vertical for OBS: vertical_get_canvas, vertical_link_scene
 * Both are optional. UI thread. */

#include "core/scene-nav.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QObject>
#include <QTimer>

#include <set>
#include <string>
#include <vector>

namespace layouts {

class SceneDirectory : public QObject {
	Q_OBJECT

public:
	static SceneDirectory &instance();

	/* Sections per canvas (main first), rebuilt when something changed. */
	const std::vector<core::nav::Node> &tree();
	/* Canvas, stacks and scene leading to this scene (pointers into tree()), empty when
	 * not found. */
	std::vector<const core::nav::Node *> locate(const std::string &sceneUuid);
	bool stacksPresent();
	std::vector<std::string> stackPaths();

	const std::set<std::string> &programScenes() const { return program_; }
	const std::set<std::string> &previewScenes() const { return preview_; }

	/* Rebuilds now (self-test) and emits changed() if needed. */
	void refreshNow();

	void handleFrontendEvent(enum obs_frontend_event event);
	/* After every module is loaded: connects to Stacks' signal if it is there. */
	void connectSisters();
	void shutdown();

	/* ---- sister plugins ---- */
	/* The organization JSON of Stacks; false when Stacks is not installed. */
	static bool stacksOrganization(std::string &json);
	/* Files a scene into a stack ("Show / Plateau"); false without Stacks or on failure. */
	static bool placeInStack(const std::string &sceneUuid, const std::string &stackPath);
	/* Vertical for OBS is installed and its canvas is ready. */
	static bool verticalReady();
	/* Links a main scene to a vertical one (Vertical's linked scenes). */
	static bool linkVertical(const QString &mainScene, const QString &verticalScene);

signals:
	void changed();        /* scenes, names, stacks, canvases */
	void talliesChanged(); /* program / preview */

private:
	SceneDirectory();
	void markDirty();
	void rebuild();
	void refreshTallies();
	static void onSourceSignal(void *data, calldata_t *cd);
	static void onStacksChanged(void *data, calldata_t *cd);

	std::vector<core::nav::Node> tree_;
	std::string signature_;
	bool dirty_ = true;
	bool posted_ = false;
	bool stacks_ = false;
	bool stacksConnected_ = false;
	std::set<std::string> program_, preview_;
	QTimer tallyTimer_;
};

} // namespace layouts
