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

#include "scene-directory.hpp"

#include "live.hpp"

#include <QMetaObject>

#include <functional>

namespace layouts {

namespace {

const char *const kSourceSignals[] = {"source_create", "source_remove", "source_rename", "source_destroy"};

void addScene(std::vector<core::nav::SceneInfo> &out, obs_source_t *s)
{
	if (!s || obs_obj_is_private(s))
		return;
	out.push_back({obs_source_get_uuid(s), obs_source_get_name(s)});
}

core::nav::CanvasInfo canvasInfo(obs_canvas_t *c, bool main)
{
	core::nav::CanvasInfo info;
	info.uuid = obs_canvas_get_uuid(c) ? obs_canvas_get_uuid(c) : "";
	info.name = obs_canvas_get_name(c) ? obs_canvas_get_name(c) : "";
	info.main = main;
	obs_video_info ovi = {};
	if (obs_canvas_get_video_info(c, &ovi)) {
		info.width = ovi.base_width;
		info.height = ovi.base_height;
	}
	return info;
}

const core::nav::Node *findPath(const std::vector<core::nav::Node> &nodes, const std::string &uuid,
				std::vector<const core::nav::Node *> &path)
{
	for (const core::nav::Node &n : nodes) {
		path.push_back(&n);
		if (n.kind == core::nav::Node::Kind::Scene && n.uuid == uuid)
			return &n;
		if (findPath(n.children, uuid, path))
			return &n;
		path.pop_back();
	}
	return nullptr;
}

} // namespace

SceneDirectory &SceneDirectory::instance()
{
	static SceneDirectory *d = new SceneDirectory();
	return *d;
}

SceneDirectory::SceneDirectory()
{
	signal_handler_t *sh = obs_get_signal_handler();
	for (const char *s : kSourceSignals)
		signal_handler_connect(sh, s, onSourceSignal, this);
	/* Fallback for what has no event (a canvas program set by another plugin). */
	tallyTimer_.setInterval(1000);
	connect(&tallyTimer_, &QTimer::timeout, this, &SceneDirectory::refreshTallies);
}

void SceneDirectory::shutdown()
{
	tallyTimer_.stop();
	signal_handler_t *sh = obs_get_signal_handler();
	for (const char *s : kSourceSignals)
		signal_handler_disconnect(sh, s, onSourceSignal, this);
	if (stacksConnected_)
		signal_handler_disconnect(sh, "stacks_changed", onStacksChanged, this);
	stacksConnected_ = false;
	tree_.clear();
}

void SceneDirectory::connectSisters()
{
	std::string json;
	if (!stacksConnected_ && stacksOrganization(json)) {
		signal_handler_connect(obs_get_signal_handler(), "stacks_changed", onStacksChanged, this);
		stacksConnected_ = true;
	}
	markDirty();
}

void SceneDirectory::onSourceSignal(void *data, calldata_t *cd)
{
	/* Any thread: only scenes matter. */
	obs_source_t *src = static_cast<obs_source_t *>(calldata_ptr(cd, "source"));
	if (!src || obs_source_get_type(src) != OBS_SOURCE_TYPE_SCENE)
		return;
	auto *self = static_cast<SceneDirectory *>(data);
	QMetaObject::invokeMethod(self, [self] { self->markDirty(); }, Qt::QueuedConnection);
}

void SceneDirectory::onStacksChanged(void *data, calldata_t *)
{
	auto *self = static_cast<SceneDirectory *>(data);
	QMetaObject::invokeMethod(self, [self] { self->markDirty(); }, Qt::QueuedConnection);
}

void SceneDirectory::markDirty()
{
	dirty_ = true;
	if (posted_)
		return;
	posted_ = true;
	/* Coalesced: a collection load renames and creates many scenes at once. */
	QTimer::singleShot(50, this, [this] {
		posted_ = false;
		rebuild();
	});
}

void SceneDirectory::refreshNow()
{
	dirty_ = true;
	rebuild();
	refreshTallies();
}

void SceneDirectory::rebuild()
{
	std::vector<core::nav::CanvasInfo> canvases;
	obs_canvas_t *mainCanvas = obs_get_main_canvas();
	if (mainCanvas) {
		core::nav::CanvasInfo main = canvasInfo(mainCanvas, true);
		obs_frontend_source_list list = {};
		obs_frontend_get_scenes(&list);
		for (size_t i = 0; i < list.sources.num; i++)
			addScene(main.scenes, list.sources.array[i]);
		obs_frontend_source_list_free(&list);
		canvases.push_back(std::move(main));
		obs_canvas_release(mainCanvas);
	}
	obs_frontend_canvas_list list = {};
	obs_frontend_get_canvases(&list);
	for (size_t i = 0; i < list.canvases.num; i++) {
		obs_canvas_t *c = list.canvases.array[i];
		if (obs_canvas_removed(c))
			continue;
		core::nav::CanvasInfo info = canvasInfo(c, false);
		obs_canvas_enum_scenes(
			c,
			[](void *p, obs_source_t *s) {
				addScene(*static_cast<std::vector<core::nav::SceneInfo> *>(p), s);
				return true;
			},
			&info.scenes);
		canvases.push_back(std::move(info));
	}
	obs_frontend_canvas_list_free(&list);

	std::string json;
	stacks_ = stacksOrganization(json);
	std::vector<core::nav::OrgNode> org;
	const bool hasOrg = stacks_ && core::nav::parseOrganization(json, org);

	/* Emit only when something visible changed. */
	std::string sig = json;
	for (const core::nav::CanvasInfo &c : canvases) {
		sig += "|" + c.uuid + ":" + c.name + ":" + std::to_string(c.width) + "x" + std::to_string(c.height);
		for (const core::nav::SceneInfo &s : c.scenes)
			sig += "/" + s.uuid + "=" + s.name;
	}
	dirty_ = false;
	if (sig == signature_)
		return;
	signature_ = sig;
	tree_ = core::nav::buildTree(canvases, hasOrg ? &org : nullptr);
	emit changed();
}

const std::vector<core::nav::Node> &SceneDirectory::tree()
{
	if (dirty_)
		rebuild();
	return tree_;
}

std::vector<const core::nav::Node *> SceneDirectory::locate(const std::string &sceneUuid)
{
	std::vector<const core::nav::Node *> path;
	if (!findPath(tree(), sceneUuid, path))
		path.clear();
	return path;
}

bool SceneDirectory::stacksPresent()
{
	tree();
	return stacks_;
}

std::vector<std::string> SceneDirectory::stackPaths()
{
	return core::nav::stackPaths(tree());
}

void SceneDirectory::refreshTallies()
{
	std::set<std::string> program, preview;
	live::tallies(program, preview);
	if (program == program_ && preview == preview_)
		return;
	program_ = std::move(program);
	preview_ = std::move(preview);
	emit talliesChanged();
}

void SceneDirectory::handleFrontendEvent(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		tallyTimer_.start();
		[[fallthrough]];
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
	case OBS_FRONTEND_EVENT_CANVAS_ADDED:
	case OBS_FRONTEND_EVENT_CANVAS_REMOVED:
		markDirty();
		refreshTallies();
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
	case OBS_FRONTEND_EVENT_TRANSITION_STOPPED:
		refreshTallies();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		shutdown();
		break;
	default:
		break;
	}
}

/* ---- sister plugins ---- */

bool SceneDirectory::stacksOrganization(std::string &json)
{
	json.clear();
	proc_handler_t *ph = obs_get_proc_handler();
	calldata_t cd;
	calldata_init(&cd);
	const bool present = ph && proc_handler_call(ph, "stacks_get_organization", &cd);
	if (present) {
		const char *j = calldata_string(&cd, "json");
		json = j ? j : "";
	}
	calldata_free(&cd);
	return present;
}

bool SceneDirectory::placeInStack(const std::string &sceneUuid, const std::string &stackPath)
{
	proc_handler_t *ph = obs_get_proc_handler();
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_string(&cd, "uuid", sceneUuid.c_str());
	calldata_set_string(&cd, "stack", stackPath.c_str());
	const bool ok = ph && proc_handler_call(ph, "stacks_place_scene", &cd) && calldata_bool(&cd, "ok");
	calldata_free(&cd);
	return ok;
}

bool SceneDirectory::verticalReady()
{
	proc_handler_t *ph = obs_get_proc_handler();
	calldata_t cd;
	calldata_init(&cd);
	const bool ready = ph && proc_handler_call(ph, "vertical_get_canvas", &cd) && calldata_bool(&cd, "ready");
	calldata_free(&cd);
	return ready;
}

bool SceneDirectory::linkVertical(const QString &mainScene, const QString &verticalScene)
{
	proc_handler_t *ph = obs_get_proc_handler();
	calldata_t cd;
	calldata_init(&cd);
	const QByteArray m = mainScene.toUtf8(), v = verticalScene.toUtf8();
	calldata_set_string(&cd, "main_scene", m.constData());
	calldata_set_string(&cd, "vertical_scene", v.constData());
	const bool ok = ph && proc_handler_call(ph, "vertical_link_scene", &cd) && calldata_bool(&cd, "success");
	calldata_free(&cd);
	return ok;
}

} // namespace layouts
