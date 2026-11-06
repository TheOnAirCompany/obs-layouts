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

#include "builder.hpp"

#include "commands.hpp"
#include "core/broadcast-screens.hpp"
#include "dual.hpp"
#include "edit-context.hpp"
#include "obs-convert.hpp"
#include "roles.hpp"
#include "scene-directory.hpp"
#include "core/slot-apply.hpp"

#include <obs-frontend-api.h>

#include <cmath>

namespace layouts::builder {

namespace {

struct FoundPlacement {
	obs_source_t *source;
	obs_transform_info *info;
	obs_sceneitem_crop *crop;
	obs_source_t *scene = nullptr;
	bool found = false;
};

bool findIn(void *p, obs_source_t *sceneSource)
{
	auto *f = static_cast<FoundPlacement *>(p);
	obs_scene_t *scene = obs_source_is_group(sceneSource) ? nullptr : obs_scene_from_source(sceneSource);
	obs_sceneitem_t *item = scene ? obs_scene_find_source_recursive(scene, obs_source_get_name(f->source))
				      : nullptr;
	if (!item)
		return true;
	obs_sceneitem_get_info2(item, f->info);
	obs_sceneitem_get_crop(item, f->crop);
	f->scene = sceneSource;
	f->found = true;
	return false;
}

/* Transform and crop of the source somewhere in the collection: first a scene of the
 * target canvas, else any scene, scaled to the target width (what sat in the lower half
 * stays at the bottom, as for the 9:16 twins). */
bool existingPlacement(obs_source_t *source, obs_transform_info &info, obs_sceneitem_crop &crop,
		       obs_canvas_t *target = nullptr)
{
	FoundPlacement f{source, &info, &crop};
	if (target)
		obs_canvas_enum_scenes(target, findIn, &f);
	if (f.found)
		return true;
	obs_enum_scenes(findIn, &f);
	if (!f.found || !target)
		return f.found;
	const core::Rect to = canvasRect(target);
	const double fromW = obs_source_get_width(f.scene), fromH = obs_source_get_height(f.scene);
	if (fromW <= 0 || fromH <= 0 || (std::fabs(to.w - fromW) < 0.5 && std::fabs(to.h - fromH) < 0.5))
		return true;
	const double k = to.w / fromW;
	const double h = obs_source_get_height(source) * std::fabs(info.scale.y);
	const double top = info.pos.y * k;
	const double fromBottom = (fromH - info.pos.y - h) * k;
	const double y = info.pos.y + h / 2 < fromH / 2 ? top : to.h - fromBottom - h * k;
	info.pos.x = (float)(info.pos.x * k);
	info.pos.y = (float)y;
	info.scale.x = (float)(info.scale.x * k);
	info.scale.y = (float)(info.scale.y * k);
	info.bounds.x = (float)(info.bounds.x * k);
	info.bounds.y = (float)(info.bounds.y * k);
	return true;
}

/* Model of a source for slotTransform: its size, and its flip where it is already used. */
core::ItemModel modelOf(obs_source_t *source)
{
	core::ItemModel m;
	m.sourceWidth = obs_source_get_width(source);
	m.sourceHeight = obs_source_get_height(source);
	obs_transform_info info = {};
	obs_sceneitem_crop crop = {};
	if (existingPlacement(source, info, crop)) {
		m.t.scale.x = info.scale.x < 0 ? -1.0 : 1.0;
		m.t.scale.y = info.scale.y < 0 ? -1.0 : 1.0;
	}
	return m;
}

/* Unique in the collection and in the target canvas (scenes of other canvases have
 * their own names). */
QString uniqueSceneName(const QString &wanted, obs_canvas_t *canvas)
{
	QString name = wanted;
	for (int i = 2;; i++) {
		const QByteArray n = name.toUtf8();
		OBSSourceAutoRelease existing = obs_get_source_by_name(n.constData());
		OBSSourceAutoRelease inCanvas = canvas ? obs_canvas_get_source_by_name(canvas, n.constData()) : nullptr;
		if (!existing && !inCanvas)
			return name;
		name = wanted + " " + QString::number(i);
	}
}

void apply(obs_sceneitem_t *item, const core::ItemTransform &t)
{
	obs_transform_info info = {};
	obs_sceneitem_crop crop = {};
	obs_sceneitem_get_info2(item, &info);
	toObs(t, info, crop);
	obs_sceneitem_set_info2(item, &info);
	obs_sceneitem_set_crop(item, &crop);
}

} // namespace

Plan plan(const std::vector<OBSSource> &sources, const Options &options, const TemplateName &templateName,
	  const SoloName &soloName)
{
	Plan p;
	std::vector<OBSSource> hosts, cams;
	for (const OBSSource &s : sources) {
		switch (roles::role(s)) {
		case core::Role::Host:
			hosts.push_back(s);
			break;
		case core::Role::Camera:
			cams.push_back(s);
			break;
		case core::Role::Screen:
			p.screens.push_back(s);
			break;
		case core::Role::Background:
			if (!p.background)
				p.background = s;
			break;
		case core::Role::Graphics:
			p.graphics.push_back(s);
			break;
		case core::Role::Chat:
			p.chats.push_back(s);
			break;
		case core::Role::None:
			break;
		}
	}
	p.cameras = hosts;
	p.cameras.insert(p.cameras.end(), cams.begin(), cams.end());

	if (options.solo) {
		for (const OBSSource &s : p.cameras)
			p.scenes.push_back(
				{"Cam1Full", soloName(QString::fromUtf8(obs_source_get_name(s))), {s}, {}, {}});
		for (const OBSSource &s : p.screens)
			p.scenes.push_back(
				{"Screen1Full", soloName(QString::fromUtf8(obs_source_get_name(s))), {}, {s}, {}});
	}
	if (options.layouts) {
		const core::Rect canvas = canvasRect(options.canvas);
		for (const core::BroadcastTemplate &t : core::templatesFor((int)p.cameras.size(), (int)p.screens.size(),
									   (int)p.chats.size(), canvas.h > canvas.w)) {
			if (t.cameras + t.screens + t.chats < 2)
				continue; /* the same as a full-screen scene */
			p.scenes.push_back({t.key, templateName(t.key), p.cameras, p.screens, p.chats});
		}
	}
	return p;
}

Plan single(const std::vector<OBSSource> &sources, const std::string &key, const QString &name)
{
	Options none;
	none.solo = false;
	none.layouts = false;
	Plan p = plan(sources, none, nullptr, nullptr);
	p.scenes.push_back({key, name, p.cameras, p.screens, p.chats});
	return p;
}

std::vector<QString> create(const Plan &plan, const Options &options, std::vector<std::string> *uuids)
{
	std::vector<QString> names;
	EditContext &ctx = EditContext::instance();
	const core::Rect canvas = canvasRect(options.canvas);
	for (const SceneSpec &spec : plan.scenes) {
		obs_canvas_t *c = options.canvas      ? obs_canvas_get_ref(options.canvas)
				  : ctx.sceneSource() ? obs_source_get_canvas(ctx.sceneSource())
						      : obs_get_main_canvas();
		const QString name = uniqueSceneName(spec.name, c);
		obs_scene_t *scene = c ? obs_canvas_scene_create(c, name.toUtf8().constData())
				       : obs_scene_create(name.toUtf8().constData());
		if (c)
			obs_canvas_release(c);
		if (!scene)
			continue;

		/* Background: covers the canvas, centered, and keeps covering it if the canvas changes. */
		if (options.background && plan.background) {
			if (obs_sceneitem_t *item = obs_scene_add(scene, plan.background)) {
				core::ItemTransform t;
				t.boundsType = core::BoundsType::ScaleOuter;
				t.bounds = {canvas.w, canvas.h};
				t.boundsAlign = core::kAlignCenter;
				t.cropToBounds = true;
				t.pos = {canvas.x, canvas.y};
				apply(item, t);
				if (options.lock)
					obs_sceneitem_set_locked(item, true);
			}
		}

		/* Tiles: host slots get the hosts, then cameras in reading order and screens. */
		const std::vector<core::Slot> tiles = commands::broadcastSlots(spec.key, canvas, options.spec);
		std::vector<obs_source_t *> sources;
		std::vector<core::SlotKind> kinds;
		std::vector<bool> hosts;
		for (const OBSSource &s : spec.cameras) {
			sources.push_back(s);
			kinds.push_back(core::SlotKind::Camera);
			hosts.push_back(roles::role(s) == core::Role::Host);
		}
		for (const OBSSource &s : spec.screens) {
			sources.push_back(s);
			kinds.push_back(core::SlotKind::Screen);
			hosts.push_back(false);
		}
		for (const OBSSource &s : spec.chats) {
			sources.push_back(s);
			kinds.push_back(core::SlotKind::Chat);
			hosts.push_back(false);
		}
		const std::vector<int> assigned = core::assignItems(tiles, kinds, hosts);
		/* Added back to front, in slot order (an inset camera stays visible). */
		for (size_t k : core::stackOrder(tiles)) {
			if (assigned[k] < 0)
				continue;
			obs_source_t *source = sources[(size_t)assigned[k]];
			obs_sceneitem_t *item = obs_scene_add(scene, source);
			if (!item)
				continue;
			apply(item, commands::slotTransform(modelOf(source), tiles[k]));
			commands::applySlotStyle(item, tiles[k]);
			if (options.lock)
				commands::lockItem(item);
		}

		/* Graphics on top, where they already are in the show. */
		if (options.graphics) {
			for (const OBSSource &g : plan.graphics) {
				obs_transform_info info = {};
				obs_sceneitem_crop crop = {};
				const bool known = existingPlacement(g, info, crop, options.canvas);
				obs_sceneitem_t *item = obs_scene_add(scene, g);
				if (!item)
					continue;
				if (known) {
					obs_sceneitem_set_info2(item, &info);
					obs_sceneitem_set_crop(item, &crop);
				}
				if (options.lock)
					obs_sceneitem_set_locked(item, true);
			}
		}
		names.push_back(name);
		if (uuids)
			uuids->push_back(obs_source_get_uuid(obs_scene_get_source(scene)));
		/* Its 9:16 twin in the vertical canvas ("Both"), linked in Vertical for OBS so a
		 * cut on the main output switches the vertical one too. The dual output setting
		 * (off by default) also makes twins. */
		if (options.twin || dual::enabled()) {
			OBSSource twin = dual::refreshTwin(obs_scene_get_source(scene), spec.key, options.twin);
			if (twin)
				SceneDirectory::linkVertical(name, QString::fromUtf8(obs_source_get_name(twin)));
		}
		obs_scene_release(scene);
	}
	if (!names.empty())
		obs_frontend_save();
	return names;
}

core::Rect canvasRect(obs_canvas_t *canvas)
{
	obs_video_info ovi = {};
	if (canvas && obs_canvas_get_video_info(canvas, &ovi) && ovi.base_width > 0 && ovi.base_height > 0)
		return {0, 0, (double)ovi.base_width, (double)ovi.base_height};
	return EditContext::instance().canvasRect();
}

} // namespace layouts::builder
