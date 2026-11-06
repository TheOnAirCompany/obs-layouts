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

#include "morph.hpp"

#include "core/morph.hpp"
#include "edit-context.hpp"
#include "edit-session.hpp"
#include "obs-convert.hpp"
#include "store.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QString>

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>

namespace layouts::morph {

namespace {

struct SavedTransitions {
	OBSSceneItem item;
	OBSSource show, hide;
	uint32_t showMs = 0, hideMs = 0;
};

struct Run {
	OBSScene scene;
	std::vector<OBSSceneItem> items; /* one per plan track */
	core::MorphPlan plan;
	uint64_t startNs = 0; /* set by the first tick: the video clock */
	uint64_t durationNs = 0;
	std::string before; /* undo data */
	std::vector<SavedTransitions> saved;
};

std::mutex g_mutex;
std::vector<std::shared_ptr<Run>> g_runs; /* guarded by g_mutex */
bool g_init = false;

std::string undoData(obs_scene_t *scene, const std::vector<OBSSceneItem> &items)
{
	OBSDataAutoRelease d = obs_data_create();
	OBSDataAutoRelease states = obs_scene_save_transform_states(scene, true);
	obs_data_set_string(d, "states", states ? obs_data_get_json(states) : "");
	obs_data_set_string(d, "scene", obs_source_get_uuid(obs_scene_get_source(scene)));
	OBSDataArrayAutoRelease vis = obs_data_array_create();
	for (const OBSSceneItem &item : items) {
		OBSDataAutoRelease v = obs_data_create();
		obs_data_set_int(v, "id", obs_sceneitem_get_id(item));
		obs_data_set_bool(v, "visible", obs_sceneitem_visible(item));
		obs_data_array_push_back(vis, v);
	}
	obs_data_set_array(d, "visible", vis);
	return obs_data_get_json(d);
}

void undoRedo(const char *json)
{
	OBSDataAutoRelease d = obs_data_create_from_json(json);
	if (!d)
		return;
	obs_scene_load_transform_states(obs_data_get_string(d, "states"));
	OBSSourceAutoRelease source = obs_get_source_by_uuid(obs_data_get_string(d, "scene"));
	obs_scene_t *scene = obs_scene_from_source(source);
	if (!scene)
		return;
	OBSDataArrayAutoRelease vis = obs_data_get_array(d, "visible");
	for (size_t i = 0, n = obs_data_array_count(vis); i < n; i++) {
		OBSDataAutoRelease v = obs_data_array_item(vis, i);
		obs_sceneitem_t *item = obs_scene_find_sceneitem_by_id(scene, obs_data_get_int(v, "id"));
		if (item)
			obs_sceneitem_set_visible(item, obs_data_get_bool(v, "visible"));
	}
	EditContext::instance().markDirty();
}

/* Graphics thread or UI thread, g_mutex held. */
void applyFrame(const Run &run, double progress)
{
	const std::vector<core::MorphFrameItem> frame = core::sampleMorph(run.plan, progress);
	std::vector<Change> changes;
	changes.reserve(frame.size());
	for (size_t i = 0; i < frame.size() && i < run.items.size(); i++)
		changes.push_back({run.items[i], frame[i].t});
	applyChangesAtomically(run.scene, changes);
}

/* UI thread: the run has reached its end state. */
void complete(const std::shared_ptr<Run> &run)
{
	for (const SavedTransitions &s : run->saved) {
		obs_sceneitem_set_transition(s.item, true, s.show);
		obs_sceneitem_set_transition(s.item, false, s.hide);
		obs_sceneitem_set_transition_duration(s.item, true, s.showMs);
		obs_sceneitem_set_transition_duration(s.item, false, s.hideMs);
	}
	const std::string after = undoData(run->scene, run->items);
	if (!run->before.empty() && after != run->before) {
		const QString label =
			QString::fromUtf8(obs_module_text("Undo.Format"))
				.arg(QString::fromUtf8(obs_module_text("Undo.Morph")),
				     QString::fromUtf8(obs_source_get_name(obs_scene_get_source(run->scene))));
		obs_frontend_add_undo_redo_action(label.toUtf8().constData(), undoRedo, undoRedo, run->before.c_str(),
						  after.c_str(), false);
	}
	EditContext::instance().markDirty();
}

void completeTask(void *param)
{
	auto *run = static_cast<std::shared_ptr<Run> *>(param);
	complete(*run);
	delete run;
}

void tick(void *, float)
{
	std::vector<std::shared_ptr<Run>> finished;
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		if (g_runs.empty())
			return;
		const uint64_t now = obs_get_video_frame_time();
		for (auto it = g_runs.begin(); it != g_runs.end();) {
			Run &run = **it;
			if (!run.startNs)
				run.startNs = now;
			const double progress = run.durationNs ? (double)(now - run.startNs) / (double)run.durationNs
							       : 1.0;
			applyFrame(run, progress);
			if (progress >= 1.0) {
				finished.push_back(*it);
				it = g_runs.erase(it);
			} else {
				++it;
			}
		}
	}
	for (const auto &run : finished)
		obs_queue_task(OBS_TASK_UI, completeTask, new std::shared_ptr<Run>(run), false);
}

/* g_mutex held. */
std::shared_ptr<Run> takeRun(obs_scene_t *scene)
{
	for (auto it = g_runs.begin(); it != g_runs.end(); ++it) {
		if ((*it)->scene == scene) {
			auto run = *it;
			g_runs.erase(it);
			return run;
		}
	}
	return nullptr;
}

core::MorphItemState stateOf(obs_sceneitem_t *item)
{
	core::MorphItemState s;
	s.id = obs_sceneitem_get_id(item);
	obs_transform_info info = {};
	obs_sceneitem_crop crop = {};
	obs_sceneitem_get_info2(item, &info);
	obs_sceneitem_get_crop(item, &crop);
	s.model.t = fromObs(info, crop);
	obs_source_t *source = obs_sceneitem_get_source(item);
	s.model.sourceWidth = obs_source_get_width(source);
	s.model.sourceHeight = obs_source_get_height(source);
	s.visible = obs_sceneitem_visible(item);
	return s;
}

void useFade(Run &run, obs_sceneitem_t *item, bool show, obs_source_t *fade, uint32_t ms)
{
	const bool known = std::any_of(run.saved.begin(), run.saved.end(),
				       [item](const SavedTransitions &s) { return s.item == item; });
	if (!known) {
		SavedTransitions s;
		s.item = item;
		s.show = obs_sceneitem_get_transition(item, true);
		s.hide = obs_sceneitem_get_transition(item, false);
		s.showMs = obs_sceneitem_get_transition_duration(item, true);
		s.hideMs = obs_sceneitem_get_transition_duration(item, false);
		run.saved.push_back(s);
	}
	obs_sceneitem_set_transition(item, show, ms ? fade : nullptr);
	obs_sceneitem_set_transition_duration(item, show, ms);
}

} // namespace

void init()
{
	if (g_init)
		return;
	obs_add_tick_callback(tick, nullptr);
	g_init = true;
}

void shutdown()
{
	finish(nullptr);
	if (g_init)
		obs_remove_tick_callback(tick, nullptr);
	g_init = false;
}

int defaultDurationMs()
{
	return Store::instance().settings().morphDurationMs;
}

bool applyAnimated(obs_scene_t *scene, const std::vector<Target> &targets, int durationMs)
{
	if (!scene)
		return false;
	if (durationMs < 0)
		durationMs = defaultDurationMs();

	std::unique_lock<std::mutex> lock(g_mutex);
	std::shared_ptr<Run> previous = takeRun(scene);

	auto run = std::make_shared<Run>();
	run->scene = scene;
	run->durationNs = (uint64_t)durationMs * 1000000ull;

	/* Current states: mid-way positions when a morph was interrupted. */
	std::vector<core::MorphItemState> current, wanted;
	std::vector<OBSSceneItem> byIndex;
	for (const Target &target : targets) {
		if (!target.item || obs_sceneitem_get_scene(target.item) != scene)
			continue;
		current.push_back(stateOf(target.item));
		core::MorphItemState w = current.back();
		w.model.t = target.t;
		w.visible = target.visible;
		wanted.push_back(w);
		byIndex.push_back(target.item);
	}
	run->plan = core::planMorph(current, wanted);
	for (const core::MorphTrack &track : run->plan.tracks) {
		for (size_t i = 0; i < wanted.size(); i++)
			if (wanted[i].id == track.id)
				run->items.push_back(byIndex[i]);
	}

	if (previous) {
		run->before = previous->before;
		run->saved = previous->saved;
	} else if (!run->plan.empty()) {
		run->before = undoData(scene, byIndex);
	}
	if (run->plan.empty()) {
		lock.unlock();
		if (previous)
			complete(previous);
		return false;
	}

	/* Fades: per-item show / hide transitions, set and triggered on the UI thread. */
	OBSSourceAutoRelease fade = obs_source_create_private("fade_transition", "Layouts fade", nullptr);
	for (size_t i = 0; i < run->plan.tracks.size(); i++) {
		const core::MorphTrack &track = run->plan.tracks[i];
		obs_sceneitem_t *item = run->items[i];
		if (track.kind == core::TrackKind::Enter) {
			applyChangesAtomically(scene, {{OBSSceneItem(item), track.to.t}});
			useFade(*run, item, true, fade, (uint32_t)durationMs);
			obs_sceneitem_set_visible(item, true);
		} else if (track.kind == core::TrackKind::Leave) {
			useFade(*run, item, false, fade, (uint32_t)durationMs);
			obs_sceneitem_set_visible(item, false);
		}
	}

	if (durationMs == 0 || !g_init) {
		applyFrame(*run, 1.0);
		lock.unlock();
		complete(run);
		return true;
	}
	g_runs.push_back(run);
	return true;
}

void finish(obs_scene_t *scene)
{
	std::vector<std::shared_ptr<Run>> done;
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		for (auto it = g_runs.begin(); it != g_runs.end();) {
			if (!scene || (*it)->scene == scene) {
				applyFrame(**it, 1.0);
				done.push_back(*it);
				it = g_runs.erase(it);
			} else {
				++it;
			}
		}
	}
	for (const auto &run : done)
		complete(run);
}

bool running(obs_scene_t *scene)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	return std::any_of(g_runs.begin(), g_runs.end(), [scene](const auto &r) { return r->scene == scene; });
}

} // namespace layouts::morph
