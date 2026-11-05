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

#include "store.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>
#include <plugin-support.h>
#include <util/platform.h>

#include <vector>

namespace layouts {

namespace {

constexpr const char *kCollectionKey = "layouts";
constexpr int kSettingsVersion = 1;

obs_data_array_t *guidesToArray(const std::vector<core::GuideDocument::GuideRecord> &records)
{
	obs_data_array_t *arr = obs_data_array_create();
	for (const auto &g : records) {
		OBSDataAutoRelease d = obs_data_create();
		obs_data_set_string(d, "axis", g.axis.c_str());
		obs_data_set_double(d, "pos", g.pos);
		obs_data_set_bool(d, "locked", g.locked);
		obs_data_array_push_back(arr, d);
	}
	return arr;
}

std::vector<core::GuideDocument::GuideRecord> guidesFromArray(obs_data_array_t *arr)
{
	std::vector<core::GuideDocument::GuideRecord> out;
	const size_t n = arr ? obs_data_array_count(arr) : 0;
	for (size_t i = 0; i < n; i++) {
		OBSDataAutoRelease d = obs_data_array_item(arr, i);
		core::GuideDocument::GuideRecord r;
		r.axis = obs_data_get_string(d, "axis");
		r.pos = obs_data_get_double(d, "pos");
		r.locked = obs_data_get_bool(d, "locked");
		out.push_back(r);
	}
	return out;
}

std::vector<std::string> aliveSceneUuids()
{
	std::vector<std::string> uuids;
	obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++)
		uuids.emplace_back(obs_source_get_uuid(scenes.sources.array[i]));
	obs_frontend_source_list_free(&scenes);

	/* Scenes of additional canvases (OBS 33 multi-canvas). */
	obs_frontend_canvas_list canvases = {};
	obs_frontend_get_canvases(&canvases);
	for (size_t i = 0; i < canvases.canvases.num; i++) {
		obs_canvas_enum_scenes(
			canvases.canvases.array[i],
			[](void *param, obs_source_t *src) {
				static_cast<std::vector<std::string> *>(param)->emplace_back(obs_source_get_uuid(src));
				return true;
			},
			&uuids);
	}
	obs_frontend_canvas_list_free(&canvases);
	return uuids;
}

void setTrack(obs_data_t *d, const char *prefix, const core::TrackGrid &t)
{
	const std::string p(prefix);
	obs_data_set_int(d, (p + "_count").c_str(), t.count);
	obs_data_set_double(d, (p + "_gutter").c_str(), t.gutter);
	obs_data_set_double(d, (p + "_margin").c_str(), t.margin);
	obs_data_set_double(d, (p + "_size").c_str(), t.trackSize);
	obs_data_set_int(d, (p + "_align").c_str(), (int)t.align);
}

void getTrack(obs_data_t *d, const char *prefix, core::TrackGrid &t)
{
	const std::string p(prefix);
	if (obs_data_has_user_value(d, (p + "_count").c_str()))
		t.count = std::clamp((int)obs_data_get_int(d, (p + "_count").c_str()), 1, 100);
	if (obs_data_has_user_value(d, (p + "_gutter").c_str()))
		t.gutter = std::clamp(obs_data_get_double(d, (p + "_gutter").c_str()), 0.0, 10000.0);
	if (obs_data_has_user_value(d, (p + "_margin").c_str()))
		t.margin = std::clamp(obs_data_get_double(d, (p + "_margin").c_str()), 0.0, 10000.0);
	if (obs_data_has_user_value(d, (p + "_size").c_str()))
		t.trackSize = std::clamp(obs_data_get_double(d, (p + "_size").c_str()), 1.0, 10000.0);
	if (obs_data_has_user_value(d, (p + "_align").c_str()))
		t.align = (core::TrackAlign)std::clamp((int)obs_data_get_int(d, (p + "_align").c_str()), 0, 3);
}

std::string settingsPath()
{
	char *path = obs_module_config_path("settings.json");
	std::string s = path ? path : "";
	bfree(path);
	return s;
}

} // namespace

Store &Store::instance()
{
	static Store *s = new Store();
	return *s;
}

void Store::notifyGuidesChanged()
{
	emit guidesChanged();
}

void Store::notifySettingsChanged(bool persist)
{
	if (persist)
		saveSettings();
	emit settingsChanged();
}

namespace {
void undoRedoGuides(const char *data)
{
	Store::instance().restoreGuidesJson(data);
	obs_frontend_save();
}
} // namespace

std::string Store::guidesJson()
{
	OBSDataAutoRelease d = obs_data_create();
	saveGuides(d);
	const char *json = obs_data_get_json(d);
	return json ? json : "";
}

void Store::restoreGuidesJson(const char *json)
{
	OBSDataAutoRelease d = obs_data_create_from_json(json);
	if (d)
		loadGuides(d); /* emits guidesChanged */
}

void Store::beginGuideEdit()
{
	guideBefore_ = guidesJson();
	guideEditing_ = true;
}

void Store::endGuideEdit(const QString &undoLabel)
{
	notifyGuidesChanged();
	if (!guideEditing_)
		return;
	guideEditing_ = false;
	const std::string after = guidesJson();
	if (after != guideBefore_) {
		obs_frontend_add_undo_redo_action(undoLabel.toUtf8().constData(), undoRedoGuides, undoRedoGuides,
						  guideBefore_.c_str(), after.c_str(), false);
		obs_frontend_save(); /* guides live in the scene collection */
	}
}

void Store::cancelGuideEdit()
{
	if (!guideEditing_)
		return;
	guideEditing_ = false;
	restoreGuidesJson(guideBefore_.c_str());
}

void Store::saveGuides(obs_data_t *data)
{
	guides_.pruneOrphans(aliveSceneUuids());
	const core::GuideDocument::Records r = guides_.toRecords();

	OBSDataAutoRelease root = obs_data_create();
	obs_data_set_int(root, "version", r.version);

	OBSDataArrayAutoRelease scenes = obs_data_array_create();
	for (const auto &s : r.scenes) {
		OBSDataAutoRelease sd = obs_data_create();
		obs_data_set_string(sd, "uuid", s.uuid.rfind("name:", 0) == 0 ? "" : s.uuid.c_str());
		obs_data_set_string(sd, "name", s.name.c_str());
		OBSDataArrayAutoRelease g = guidesToArray(s.guides);
		obs_data_set_array(sd, "guides", g);
		obs_data_array_push_back(scenes, sd);
	}
	obs_data_set_array(root, "scenes", scenes);
	OBSDataArrayAutoRelease global = guidesToArray(r.global);
	obs_data_set_array(root, "global", global);

	obs_data_set_obj(data, kCollectionKey, root);
}

void Store::loadGuides(obs_data_t *data)
{
	OBSDataAutoRelease root = obs_data_get_obj(data, kCollectionKey);
	core::GuideDocument::Records r;
	if (root) {
		r.version = (int)obs_data_get_int(root, "version");
		OBSDataArrayAutoRelease scenes = obs_data_get_array(root, "scenes");
		const size_t n = scenes ? obs_data_array_count(scenes) : 0;
		for (size_t i = 0; i < n; i++) {
			OBSDataAutoRelease sd = obs_data_array_item(scenes, i);
			core::GuideDocument::SceneRecord s;
			s.uuid = obs_data_get_string(sd, "uuid");
			s.name = obs_data_get_string(sd, "name");
			OBSDataArrayAutoRelease g = obs_data_get_array(sd, "guides");
			s.guides = guidesFromArray(g);
			r.scenes.push_back(std::move(s));
		}
		OBSDataArrayAutoRelease global = obs_data_get_array(root, "global");
		r.global = guidesFromArray(global);
	} else {
		r.version = core::kGuidesSchemaVersion;
	}

	const int rejected = guides_.fromRecords(r);
	if (rejected > 0)
		obs_log(LOG_WARNING, "ignored %d invalid guide entries in the scene collection", rejected);

	/* Re-attach guides saved without a usable UUID to scenes with the same name. */
	obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++) {
		obs_source_t *s = scenes.sources.array[i];
		const std::string uuid = obs_source_get_uuid(s);
		const std::string name = obs_source_get_name(s);
		if (guides_.sceneGuides(uuid).empty())
			guides_.adoptByName(uuid, name);
	}
	obs_frontend_source_list_free(&scenes);

	emit guidesChanged();
}

void Store::loadSettings()
{
	const std::string path = settingsPath();
	if (path.empty())
		return;
	OBSDataAutoRelease d = obs_data_create_from_json_file_safe(path.c_str(), "bak");
	if (!d)
		return;

	ViewSettings &s = settings_;
	auto b = [&](const char *k, bool &v) {
		if (obs_data_has_user_value(d, k))
			v = obs_data_get_bool(d, k);
	};
	auto f = [&](const char *k, double &v, double lo, double hi) {
		if (obs_data_has_user_value(d, k))
			v = std::clamp(obs_data_get_double(d, k), lo, hi);
	};

	b("rulers", s.rulers);
	b("guides", s.guides);
	b("grid", s.grid);
	b("safe_areas", s.safeAreas);
	b("center_lines", s.centerLines);
	b("outlines", s.outlines);
	if (obs_data_has_user_value(d, "grid_kind"))
		s.gridKind = (GridKind)std::clamp((int)obs_data_get_int(d, "grid_kind"), 0, 2);
	f("square_size", s.square.size, 1.0, 10000.0);
	f("square_offset_x", s.square.offsetX, -10000.0, 10000.0);
	f("square_offset_y", s.square.offsetY, -10000.0, 10000.0);
	getTrack(d, "columns", s.columns);
	getTrack(d, "rows", s.rows);
	b("snap_enabled", s.snap.enabled);
	b("snap_guides", s.snap.guides);
	b("snap_grid", s.snap.grid);
	b("snap_items", s.snap.items);
	b("snap_canvas", s.snap.canvas);
	b("snap_spacing", s.snap.spacing);
	f("snap_threshold", s.snap.thresholdPx, 1.0, 50.0);
	f("custom_margin_x", s.customMarginX, 0.0, 10000.0);
	f("custom_margin_y", s.customMarginY, 0.0, 10000.0);
	if (obs_data_has_user_value(d, "ref_point"))
		s.refPoint = std::clamp((int)obs_data_get_int(d, "ref_point"), 0, 8);
	b("block_on_air", s.blockOnAir);
	b("ratio_lock", s.ratioLock);
	b("first_run_done", s.firstRunDone);
	if (obs_data_has_user_value(d, "flow_direction"))
		s.flow.direction = (core::FlowDirection)std::clamp((int)obs_data_get_int(d, "flow_direction"), 0, 2);
	f("flow_gap", s.flow.gap, -10000.0, 10000.0);
	f("flow_padding", s.flow.padding, 0.0, 10000.0);
	if (obs_data_has_user_value(d, "flow_main_align"))
		s.flow.mainAlign = (core::FlowAlign)std::clamp((int)obs_data_get_int(d, "flow_main_align"), 0, 2);
	if (obs_data_has_user_value(d, "flow_cross_align"))
		s.flow.crossAlign = (core::FlowAlign)std::clamp((int)obs_data_get_int(d, "flow_cross_align"), 0, 2);
	if (obs_data_has_user_value(d, "flow_sizing"))
		s.flow.sizing = (core::FlowSizing)std::clamp((int)obs_data_get_int(d, "flow_sizing"), 0, 1);
	if (obs_data_has_user_value(d, "flow_columns"))
		s.flow.columns = std::clamp((int)obs_data_get_int(d, "flow_columns"), 1, 50);
	b("flow_in_canvas", s.flowInCanvas);
	f("preset_margin", s.preset.margin, 0.0, 10000.0);
	f("preset_gap", s.preset.gap, 0.0, 10000.0);
	f("preset_corner_scale", s.preset.cornerScale, 0.05, 1.0);
	f("broadcast_gap", s.broadcastGap, 0.0, 1000.0);
	f("broadcast_margin", s.broadcastMargin, 0.0, 1000.0);
	b("broadcast_lock", s.broadcastLock);
	b("broadcast_mirror_h", s.broadcastMirrorH);
	b("broadcast_mirror_v", s.broadcastMirrorV);
	b("builder_background", s.builderBackground);
	b("builder_graphics", s.builderGraphics);
	if (obs_data_has_user_value(d, "broadcast_kinds"))
		s.broadcastKinds = std::clamp((int)obs_data_get_int(d, "broadcast_kinds"), 0, 2);
	s.windowGeometry = QByteArray::fromBase64(obs_data_get_string(d, "window_geometry"));
	s.splitterState = QByteArray::fromBase64(obs_data_get_string(d, "splitter_state_3"));
	if (obs_data_has_user_value(d, "guide_color")) {
		const QColor c(QString::fromUtf8(obs_data_get_string(d, "guide_color")));
		if (c.isValid())
			s.guideColor = c;
	}
}

void Store::saveSettings()
{
	const std::string path = settingsPath();
	if (path.empty())
		return;
	char *dir = obs_module_config_path("");
	if (dir) {
		os_mkdirs(dir);
		bfree(dir);
	}

	const ViewSettings &s = settings_;
	OBSDataAutoRelease d = obs_data_create();
	obs_data_set_int(d, "version", kSettingsVersion);
	obs_data_set_bool(d, "rulers", s.rulers);
	obs_data_set_bool(d, "guides", s.guides);
	obs_data_set_bool(d, "grid", s.grid);
	obs_data_set_bool(d, "safe_areas", s.safeAreas);
	obs_data_set_bool(d, "center_lines", s.centerLines);
	obs_data_set_bool(d, "outlines", s.outlines);
	obs_data_set_int(d, "grid_kind", (int)s.gridKind);
	obs_data_set_double(d, "square_size", s.square.size);
	obs_data_set_double(d, "square_offset_x", s.square.offsetX);
	obs_data_set_double(d, "square_offset_y", s.square.offsetY);
	setTrack(d, "columns", s.columns);
	setTrack(d, "rows", s.rows);
	obs_data_set_bool(d, "snap_enabled", s.snap.enabled);
	obs_data_set_bool(d, "snap_guides", s.snap.guides);
	obs_data_set_bool(d, "snap_grid", s.snap.grid);
	obs_data_set_bool(d, "snap_items", s.snap.items);
	obs_data_set_bool(d, "snap_canvas", s.snap.canvas);
	obs_data_set_bool(d, "snap_spacing", s.snap.spacing);
	obs_data_set_double(d, "snap_threshold", s.snap.thresholdPx);
	obs_data_set_double(d, "custom_margin_x", s.customMarginX);
	obs_data_set_double(d, "custom_margin_y", s.customMarginY);
	obs_data_set_int(d, "ref_point", s.refPoint);
	obs_data_set_bool(d, "block_on_air", s.blockOnAir);
	obs_data_set_bool(d, "ratio_lock", s.ratioLock);
	obs_data_set_bool(d, "first_run_done", s.firstRunDone);
	obs_data_set_int(d, "flow_direction", (int)s.flow.direction);
	obs_data_set_double(d, "flow_gap", s.flow.gap);
	obs_data_set_double(d, "flow_padding", s.flow.padding);
	obs_data_set_int(d, "flow_main_align", (int)s.flow.mainAlign);
	obs_data_set_int(d, "flow_cross_align", (int)s.flow.crossAlign);
	obs_data_set_int(d, "flow_sizing", (int)s.flow.sizing);
	obs_data_set_int(d, "flow_columns", s.flow.columns);
	obs_data_set_bool(d, "flow_in_canvas", s.flowInCanvas);
	obs_data_set_double(d, "preset_margin", s.preset.margin);
	obs_data_set_double(d, "preset_gap", s.preset.gap);
	obs_data_set_double(d, "preset_corner_scale", s.preset.cornerScale);
	obs_data_set_double(d, "broadcast_gap", s.broadcastGap);
	obs_data_set_double(d, "broadcast_margin", s.broadcastMargin);
	obs_data_set_bool(d, "broadcast_lock", s.broadcastLock);
	obs_data_set_bool(d, "broadcast_mirror_h", s.broadcastMirrorH);
	obs_data_set_bool(d, "broadcast_mirror_v", s.broadcastMirrorV);
	obs_data_set_bool(d, "builder_background", s.builderBackground);
	obs_data_set_bool(d, "builder_graphics", s.builderGraphics);
	obs_data_set_int(d, "broadcast_kinds", s.broadcastKinds);
	obs_data_set_string(d, "window_geometry", s.windowGeometry.toBase64().constData());
	obs_data_set_string(d, "splitter_state_3", s.splitterState.toBase64().constData());
	obs_data_set_string(d, "guide_color", s.guideColor.name(QColor::HexRgb).toUtf8().constData());

	if (!obs_data_save_json_safe(d, path.c_str(), "tmp", "bak"))
		obs_log(LOG_WARNING, "could not save settings to %s", path.c_str());
}

} // namespace layouts
