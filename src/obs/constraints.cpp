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

#include "constraints.hpp"

#include "edit-context.hpp"
#include "edit-session.hpp"
#include "obs-convert.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>
#include <plugin-support.h>

#include <QMetaObject>

#include <algorithm>
#include <vector>

namespace layouts {

namespace {

constexpr const char *kKeyH = "layouts_constraint_h";
constexpr const char *kKeyV = "layouts_constraint_v";

void onGlobalCanvasReset(void *, calldata_t *cd)
{
	obs_canvas_t *canvas = static_cast<obs_canvas_t *>(calldata_ptr(cd, "canvas"));
	if (!canvas)
		return;
	const QString uuid = QString::fromUtf8(obs_canvas_get_uuid(canvas));
	QMetaObject::invokeMethod(
		&ConstraintWatcher::instance(), [uuid] { ConstraintWatcher::instance().onCanvasReset(uuid); },
		Qt::QueuedConnection);
}

bool canvasSize(obs_canvas_t *canvas, uint32_t &w, uint32_t &h)
{
	obs_video_info ovi = {};
	if (!canvas || !obs_canvas_get_video_info(canvas, &ovi))
		return false;
	w = ovi.base_width;
	h = ovi.base_height;
	return w > 0 && h > 0;
}

struct ItemData {
	OBSSceneItem item;
	core::ItemModel model;
	core::HConstraint h;
	core::VConstraint v;
	bool scales; /* OBS rescales the item with the canvas height */
};

bool collectItem(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	core::HConstraint h;
	core::VConstraint v;
	getConstraints(item, h, v);
	if (h == core::HConstraint::None && v == core::VConstraint::None)
		return true;
	ItemData d;
	d.item = item;
	obs_transform_info info = {};
	obs_sceneitem_crop crop = {};
	obs_sceneitem_get_info2(item, &info);
	obs_sceneitem_get_crop(item, &crop);
	d.model.t = fromObs(info, crop);
	obs_source_t *src = obs_sceneitem_get_source(item);
	d.model.sourceWidth = obs_source_get_width(src);
	d.model.sourceHeight = obs_source_get_height(src);
	d.h = h;
	d.v = v;
	d.scales = !obs_sceneitem_is_group(item) && !obs_source_is_scene(src);
	static_cast<std::vector<ItemData> *>(param)->push_back(d);
	return true;
}

} // namespace

void getConstraints(obs_sceneitem_t *item, core::HConstraint &h, core::VConstraint &v)
{
	h = core::HConstraint::None;
	v = core::VConstraint::None;
	if (!item)
		return;
	OBSDataAutoRelease d = obs_sceneitem_get_private_settings(item);
	if (!d)
		return;
	h = (core::HConstraint)std::clamp((int)obs_data_get_int(d, kKeyH), 0, 5);
	v = (core::VConstraint)std::clamp((int)obs_data_get_int(d, kKeyV), 0, 5);
}

void setConstraints(obs_sceneitem_t *item, core::HConstraint h, core::VConstraint v)
{
	if (!item)
		return;
	OBSDataAutoRelease d = obs_sceneitem_get_private_settings(item);
	if (!d)
		return;
	if (h == core::HConstraint::None)
		obs_data_erase(d, kKeyH);
	else
		obs_data_set_int(d, kKeyH, (int)h);
	if (v == core::VConstraint::None)
		obs_data_erase(d, kKeyV);
	else
		obs_data_set_int(d, kKeyV, (int)v);
}

ConstraintWatcher &ConstraintWatcher::instance()
{
	static ConstraintWatcher *w = new ConstraintWatcher();
	return *w;
}

void ConstraintWatcher::start()
{
	if (started_)
		return;
	signal_handler_connect(obs_get_signal_handler(), "canvas_video_reset", onGlobalCanvasReset, nullptr);
	started_ = true;
	recordSizes();
}

void ConstraintWatcher::stop()
{
	if (!started_)
		return;
	signal_handler_disconnect(obs_get_signal_handler(), "canvas_video_reset", onGlobalCanvasReset, nullptr);
	started_ = false;
}

void ConstraintWatcher::recordSizes()
{
	auto record = [this](obs_canvas_t *c) {
		uint32_t w, h;
		if (canvasSize(c, w, h))
			sizes_[obs_canvas_get_uuid(c)] = {w, h};
	};
	obs_canvas_t *main = obs_get_main_canvas();
	record(main);
	obs_canvas_release(main);
	obs_frontend_canvas_list list = {};
	obs_frontend_get_canvases(&list);
	for (size_t i = 0; i < list.canvases.num; i++)
		record(list.canvases.array[i]);
	obs_frontend_canvas_list_free(&list);
}

void ConstraintWatcher::onCanvasReset(const QString &uuid)
{
	obs_canvas_t *canvas = obs_get_canvas_by_uuid(uuid.toUtf8().constData());
	if (!canvas)
		return;
	uint32_t w1, h1;
	const bool ok = canvasSize(canvas, w1, h1);
	auto it = sizes_.find(uuid.toStdString());
	if (!ok || it == sizes_.end()) {
		if (ok)
			sizes_[uuid.toStdString()] = {w1, h1};
		obs_canvas_release(canvas);
		return;
	}
	const uint32_t w0 = it->second.first, h0 = it->second.second;
	it->second = {w1, h1};
	if (w0 == w1 && h0 == h1) {
		obs_canvas_release(canvas);
		return;
	}

	std::vector<OBSSource> scenes;
	obs_canvas_enum_scenes(
		canvas,
		[](void *param, obs_source_t *src) {
			static_cast<std::vector<OBSSource> *>(param)->emplace_back(src);
			return true;
		},
		&scenes);
	obs_canvas_release(canvas);

	int applied = 0;
	for (const OBSSource &sceneSource : scenes) {
		obs_scene_t *scene = obs_scene_from_source(sceneSource);
		std::vector<ItemData> items;
		obs_scene_enum_items(scene, collectItem, &items);
		std::vector<Change> changes;
		const core::RefPoint center{0.5, 0.5};
		for (const ItemData &d : items) {
			const core::VisualFrame f = core::visualFrame(d.model);
			const core::Rect after = core::Frame{f.center(), f.width, f.height}.rect();
			const core::Rect before = core::invertObsRelativeMapping(after, w0, h0, w1, h1, d.scales);
			core::Rect target = core::applyConstraints(before, w0, h0, w1, h1, d.h, d.v);
			/* Axes without a constraint keep what OBS did. */
			if (d.h == core::HConstraint::None) {
				target.x = after.x;
				target.w = after.w;
			}
			if (d.v == core::VConstraint::None) {
				target.y = after.y;
				target.h = after.h;
			}
			core::ItemModel m = d.model;
			m.t = core::setWorldSize(m, core::Affine::identity(), target.w, target.h, center);
			m.t = core::moveRefTo(m, core::Affine::identity(), center, target.center());
			changes.push_back({d.item, m.t});
		}
		if (!changes.empty()) {
			applyChangesAtomically(scene, changes);
			applied += (int)changes.size();
		}
	}
	if (applied > 0) {
		obs_log(LOG_INFO, "applied constraints to %d items after a canvas change from %ux%u to %ux%u", applied,
			w0, h0, w1, h1);
		EditContext::instance().markDirty();
	}
}

} // namespace layouts
