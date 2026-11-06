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

#include "dual.hpp"

#include "commands.hpp"
#include "obs-convert.hpp"
#include "roles.hpp"
#include "store.hpp"
#include "core/portrait-map.hpp"

#include <obs-frontend-api.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace layouts::dual {

namespace {

bool canvasSize(obs_canvas_t *c, uint32_t &w, uint32_t &h)
{
	obs_video_info ovi = {};
	if (!c || !obs_canvas_get_video_info(c, &ovi))
		return false;
	w = ovi.base_width;
	h = ovi.base_height;
	return w > 0 && h > 0;
}

bool sizeOf(obs_source_t *sceneSource, double &w, double &h)
{
	obs_canvas_t *c = obs_source_get_canvas(sceneSource);
	if (!c)
		c = obs_get_main_canvas();
	uint32_t cw = 0, ch = 0;
	const bool ok = canvasSize(c, cw, ch);
	obs_canvas_release(c);
	w = cw;
	h = ch;
	return ok;
}

struct Placed {
	obs_sceneitem_t *item;
	core::Rect box; /* on the 16:9 canvas */
};

core::Rect boxOf(obs_sceneitem_t *item)
{
	matrix4 m;
	obs_sceneitem_get_box_transform(item, &m);
	/* Axis-aligned box of the unit square through the box transform. */
	const double x0 = m.t.x, y0 = m.t.y;
	const double x1 = x0 + m.x.x + m.y.x, y1 = y0 + m.x.y + m.y.y;
	return core::Rect::fromEdges(std::min(x0, x1), std::min(y0, y1), std::max(x0, x1), std::max(y0, y1));
}

/* Reading order: rows top to bottom (2 px tolerance), then left to right. */
void readingOrder(std::vector<Placed> &list)
{
	std::stable_sort(list.begin(), list.end(), [](const Placed &a, const Placed &b) {
		if (std::fabs(a.box.y - b.box.y) > 2)
			return a.box.y < b.box.y;
		return a.box.x < b.box.x;
	});
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

void clearScene(obs_scene_t *scene)
{
	std::vector<OBSSceneItem> items;
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *p) {
			static_cast<std::vector<OBSSceneItem> *>(p)->push_back(item);
			return true;
		},
		&items);
	for (const OBSSceneItem &i : items)
		obs_sceneitem_remove(i);
}

} // namespace

bool enabled()
{
	return Store::instance().settings().dualEnabled;
}

QString twinName(const QString &sceneName)
{
	return sceneName + QString::fromUtf8(" \xE2\x96\xAF");
}

obs_canvas_t *verticalCanvas(bool)
{
	/* Vertical for OBS owns the 9:16 canvas (outputs, virtual camera): ask it first. */
	proc_handler_t *ph = obs_get_proc_handler();
	calldata_t cd;
	calldata_init(&cd);
	obs_canvas_t *found = nullptr;
	if (ph && proc_handler_call(ph, "vertical_get_canvas", &cd)) {
		const char *uuid = calldata_string(&cd, "uuid");
		if (uuid && *uuid)
			found = obs_get_canvas_by_uuid(uuid);
	}
	calldata_free(&cd);
	if (found)
		return found;
	/* Otherwise an existing portrait canvas, preferring one named "vertical". Layouts
	 * never creates one itself. */
	obs_canvas_t *named = nullptr, *portrait = nullptr;
	obs_frontend_canvas_list list = {};
	obs_frontend_get_canvases(&list);
	for (size_t i = 0; i < list.canvases.num; i++) {
		obs_canvas_t *c = list.canvases.array[i];
		uint32_t w = 0, h = 0;
		if (obs_canvas_removed(c) || !canvasSize(c, w, h) || h <= w)
			continue;
		const QString name = QString::fromUtf8(obs_canvas_get_name(c));
		if (!named && name.contains("vertical", Qt::CaseInsensitive))
			named = c;
		if (!portrait)
			portrait = c;
	}
	found = named ? named : portrait;
	if (found)
		found = obs_canvas_get_ref(found);
	obs_frontend_canvas_list_free(&list);
	return found;
}

OBSSource refreshTwin(obs_source_t *landscapeScene, const std::string &key, bool force)
{
	if ((!enabled() && !force) || !landscapeScene)
		return nullptr;
	obs_scene_t *src = obs_scene_from_source(landscapeScene);
	double lw = 0, lh = 0;
	if (!src || !sizeOf(landscapeScene, lw, lh))
		return nullptr;

	/* What the 16:9 scene shows, by role. */
	std::vector<Placed> cameras, screens, backgrounds, graphics;
	struct Collect {
		std::vector<Placed> *cameras, *screens, *backgrounds, *graphics;
		std::vector<bool> hosts;
	} col{&cameras, &screens, &backgrounds, &graphics, {}};
	obs_scene_enum_items(
		src,
		[](obs_scene_t *, obs_sceneitem_t *item, void *p) {
			auto *c = static_cast<Collect *>(p);
			if (!obs_sceneitem_visible(item))
				return true;
			obs_source_t *s = obs_sceneitem_get_source(item);
			const Placed placed{item, boxOf(item)};
			switch (roles::role(s)) {
			case core::Role::Host:
			case core::Role::Camera:
				c->cameras->push_back(placed);
				break;
			case core::Role::Screen:
				c->screens->push_back(placed);
				break;
			case core::Role::Background:
				c->backgrounds->push_back(placed);
				break;
			case core::Role::Graphics:
				c->graphics->push_back(placed);
				break;
			default:
				break;
			}
			return true;
		},
		&col);
	readingOrder(cameras);
	readingOrder(screens);

	std::vector<core::Slot> land;
	for (const Placed &p : cameras) {
		core::Slot s{core::SlotKind::Camera, p.box};
		s.host = roles::role(obs_sceneitem_get_source(p.item)) == core::Role::Host;
		land.push_back(s);
	}
	for (const Placed &p : screens)
		land.push_back({core::SlotKind::Screen, p.box});

	obs_canvas_t *canvas = verticalCanvas(true);
	uint32_t pw = 0, ph = 0;
	if (!canvas || !canvasSize(canvas, pw, ph)) {
		obs_canvas_release(canvas);
		return nullptr;
	}
	const core::Rect portrait{0, 0, (double)pw, (double)ph};
	const ViewSettings &vs = Store::instance().settings();
	core::BroadcastSpec spec;
	spec.gap = vs.broadcastGap;
	spec.margin = vs.broadcastMargin;
	const core::PortraitMap map = core::mapToPortrait(key, land, portrait, spec);

	/* The twin: same name plus the vertical mark, emptied and refilled. */
	const QByteArray name = twinName(QString::fromUtf8(obs_source_get_name(landscapeScene))).toUtf8();
	obs_scene_t *twin = obs_canvas_get_scene_by_name(canvas, name.constData());
	if (twin)
		clearScene(twin);
	else
		twin = obs_canvas_scene_create(canvas, name.constData());
	obs_canvas_release(canvas);
	if (!twin)
		return nullptr;

	const bool lock = vs.broadcastLock;
	for (const Placed &b : backgrounds) {
		obs_sceneitem_t *item = obs_scene_add(twin, obs_sceneitem_get_source(b.item));
		if (!item)
			continue;
		core::ItemTransform t;
		t.boundsType = core::BoundsType::ScaleOuter;
		t.bounds = {portrait.w, portrait.h};
		t.boundsAlign = core::kAlignCenter;
		t.cropToBounds = true;
		apply(item, t);
		if (lock)
			obs_sceneitem_set_locked(item, true);
	}

	/* Screens below, cameras above (the picture-in-picture camera stays visible). */
	const size_t nCams = cameras.size();
	for (int pass = 0; pass < 2; pass++) {
		for (size_t i = 0; i < land.size(); i++) {
			const bool camera = i < nCams;
			if ((pass == 0) == camera || map.twinOf[i] < 0)
				continue;
			obs_source_t *s = obs_sceneitem_get_source(camera ? cameras[i].item : screens[i - nCams].item);
			obs_sceneitem_t *item = obs_scene_add(twin, s);
			if (!item)
				continue;
			core::ItemModel m;
			m.sourceWidth = obs_source_get_width(s);
			m.sourceHeight = obs_source_get_height(s);
			const core::Slot &slot = map.tiles[map.twinOf[i]];
			apply(item, commands::slotTransform(m, slot.kind, slot.cell));
			if (lock)
				commands::lockItem(item);
		}
	}

	/* Graphics on top, scaled to the width; what sat in the lower half stays at the bottom. */
	const double k = lw > 0 ? portrait.w / lw : 1;
	for (const Placed &g : graphics) {
		obs_sceneitem_t *item = obs_scene_add(twin, obs_sceneitem_get_source(g.item));
		if (!item)
			continue;
		obs_transform_info info = {};
		obs_sceneitem_crop crop = {};
		obs_sceneitem_get_info2(g.item, &info);
		obs_sceneitem_get_crop(g.item, &crop);
		const double top = g.box.y * k;
		const double fromBottom = (lh - g.box.bottom()) * k;
		const double y = g.box.y + g.box.h / 2 < lh / 2 ? top : portrait.h - fromBottom - g.box.h * k;
		info.pos.x = (float)(info.pos.x * k);
		info.pos.y = (float)(info.pos.y * k + (y - top));
		info.scale.x = (float)(info.scale.x * k);
		info.scale.y = (float)(info.scale.y * k);
		info.bounds.x = (float)(info.bounds.x * k);
		info.bounds.y = (float)(info.bounds.y * k);
		obs_sceneitem_set_info2(item, &info);
		obs_sceneitem_set_crop(item, &crop);
		if (lock)
			obs_sceneitem_set_locked(item, true);
	}

	OBSSource out = obs_scene_get_source(twin);
	obs_scene_release(twin);
	return out;
}

bool cutTwin(obs_source_t *landscapeScene)
{
	if (!landscapeScene)
		return false;
	obs_canvas_t *canvas = verticalCanvas(false);
	if (!canvas)
		return false;
	const QByteArray name = twinName(QString::fromUtf8(obs_source_get_name(landscapeScene))).toUtf8();
	obs_scene_t *twin = obs_canvas_get_scene_by_name(canvas, name.constData());
	if (twin) {
		obs_canvas_set_channel(canvas, 0, obs_scene_get_source(twin));
		obs_scene_release(twin);
	}
	obs_canvas_release(canvas);
	return twin != nullptr;
}

} // namespace layouts::dual
