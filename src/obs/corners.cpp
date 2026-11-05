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

#include "corners.hpp"

#include "edit-context.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <graphics/matrix4.h>
#include <util/util.hpp>

#include <QString>

#include <algorithm>
#include <cmath>
#include <vector>

namespace layouts::corners {

namespace {

QString tr(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

/* ---- the filter ------------------------------------------------------------- */

struct Filter {
	obs_source_t *context = nullptr;
	gs_effect_t *effect = nullptr;
	gs_eparam_t *size = nullptr;
	gs_eparam_t *rect = nullptr;
	gs_eparam_t *radius = nullptr;
	float r = 0.0f;     /* source pixels, horizontal */
	float ratio = 1.0f; /* vertical radius / horizontal radius (non-uniform item scale) */
};

const char *filterName(void *)
{
	return obs_module_text("Corners.FilterName");
}

void filterUpdate(void *data, obs_data_t *settings)
{
	auto *f = static_cast<Filter *>(data);
	f->r = (float)std::max(0.0, obs_data_get_double(settings, "radius"));
	const double ratio = obs_data_get_double(settings, "ratio");
	f->ratio = (float)(ratio > 0.0 ? ratio : 1.0);
}

void *filterCreate(obs_data_t *settings, obs_source_t *context)
{
	auto *f = new Filter;
	f->context = context;
	BPtr<char> path = obs_module_file("effects/rounded-corners.effect");
	obs_enter_graphics();
	f->effect = path ? gs_effect_create_from_file(path, nullptr) : nullptr;
	if (f->effect) {
		f->size = gs_effect_get_param_by_name(f->effect, "size");
		f->rect = gs_effect_get_param_by_name(f->effect, "rect");
		f->radius = gs_effect_get_param_by_name(f->effect, "radius");
	}
	obs_leave_graphics();
	if (!f->effect)
		blog(LOG_WARNING, "[layouts] rounded corners effect could not be loaded");
	filterUpdate(f, settings);
	return f;
}

void filterDestroy(void *data)
{
	auto *f = static_cast<Filter *>(data);
	obs_enter_graphics();
	gs_effect_destroy(f->effect);
	obs_leave_graphics();
	delete f;
}

void filterDefaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, "radius", 24.0);
	obs_data_set_default_double(settings, "ratio", 1.0);
}

obs_properties_t *filterProperties(void *)
{
	obs_properties_t *props = obs_properties_create();
	obs_properties_add_float_slider(props, "radius", obs_module_text("Corners.Radius"), 0.0, 1000.0, 1.0);
	return props;
}

bool near(float a, float b)
{
	return std::fabs(a - b) <= 1e-3f * std::max(1.0f, std::fabs(b));
}

/* Part of the source that the scene item being drawn shows, in source pixels.
 *
 * A cropped scene item is drawn into a texture the size of its visible part: the
 * viewport is that size and the matrix maps the source onto it (scale, then a shift by
 * the crop). Anything else (no crop, filter preview, projector) shows the whole source. */
void visibleRect(float w, float h, float &left, float &top, float &right, float &bottom)
{
	left = 0.0f;
	top = 0.0f;
	right = w;
	bottom = h;
	struct matrix4 m;
	gs_matrix_get(&m);
	struct gs_rect vp;
	gs_get_viewport(&vp);
	if (vp.cx <= 0 || vp.cy <= 0)
		return;
	const float sx = w / (float)vp.cx, sy = h / (float)vp.cy;
	if (!near(m.x.x, sx) || !near(m.y.y, sy) || std::fabs(m.x.y) > 1e-4f || std::fabs(m.y.x) > 1e-4f ||
	    m.t.x > 0.01f || m.t.y > 0.01f)
		return;
	const float l = -m.t.x / sx, t = -m.t.y / sy;
	if (l + (float)vp.cx > w + 0.5f || t + (float)vp.cy > h + 0.5f)
		return;
	left = l;
	top = t;
	right = l + (float)vp.cx;
	bottom = t + (float)vp.cy;
}

const enum gs_color_space kSpaces[] = {GS_CS_SRGB, GS_CS_SRGB_16F, GS_CS_709_EXTENDED};

void filterRender(void *data, gs_effect_t *)
{
	auto *f = static_cast<Filter *>(data);
	obs_source_t *target = obs_filter_get_target(f->context);
	const uint32_t w = target ? obs_source_get_width(target) : 0;
	const uint32_t h = target ? obs_source_get_height(target) : 0;
	if (!f->effect || f->r < 0.5f || !w || !h) {
		obs_source_skip_video_filter(f->context);
		return;
	}
	/* Read before the filter renders its input (which sets its own matrix). */
	float l, t, r, b;
	visibleRect((float)w, (float)h, l, t, r, b);

	const enum gs_color_space space = obs_source_get_color_space(target, OBS_COUNTOF(kSpaces), kSpaces);
	if (!obs_source_process_filter_begin_with_color_space(f->context, gs_get_format_from_space(space), space,
							      OBS_ALLOW_DIRECT_RENDERING))
		return;
	struct vec2 size, radius;
	vec2_set(&size, (float)w, (float)h);
	vec2_set(&radius, f->r, f->r * f->ratio);
	struct vec4 rect;
	vec4_set(&rect, l, t, r, b);
	gs_effect_set_vec2(f->size, &size);
	gs_effect_set_vec4(f->rect, &rect);
	gs_effect_set_vec2(f->radius, &radius);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	obs_source_process_filter_end(f->context, f->effect, 0, 0);
	gs_blend_state_pop();
}

enum gs_color_space filterColorSpace(void *data, size_t, const enum gs_color_space *)
{
	auto *f = static_cast<Filter *>(data);
	obs_source_t *target = obs_filter_get_target(f->context);
	return target ? obs_source_get_color_space(target, OBS_COUNTOF(kSpaces), kSpaces) : GS_CS_SRGB;
}

/* ---- editing ------------------------------------------------------------------ */

obs_source_t *findFilter(obs_source_t *source)
{
	obs_source_t *found = nullptr;
	obs_source_enum_filters(
		source,
		[](obs_source_t *, obs_source_t *filter, void *p) {
			auto **out = static_cast<obs_source_t **>(p);
			if (!*out && strcmp(obs_source_get_unversioned_id(filter), kFilterId) == 0)
				*out = filter;
		},
		&found);
	return found; /* weak: valid while the source keeps it */
}

/* World scale of the source pixels of an item (its own scale, bounds and group). */
core::Vec2 worldScale(const ItemEntry &e)
{
	const core::ItemGeometry g = core::computeGeometry(e.model);
	const core::Vec2 x = e.parent.mapVector({g.outputScale.x, 0.0});
	const core::Vec2 y = e.parent.mapVector({0.0, g.outputScale.y});
	return {std::hypot(x.x, x.y), std::hypot(y.x, y.y)};
}

/* Settings of the filter of a source: radius in source pixels and vertical ratio. */
void sourceRadius(obs_source_t *source, double &r, double &ratio)
{
	r = 0.0;
	ratio = 1.0;
	if (obs_source_t *filter = findFilter(source)) {
		OBSDataAutoRelease s = obs_source_get_settings(filter);
		r = obs_data_get_double(s, "radius");
		ratio = obs_data_get_double(s, "ratio");
		if (ratio <= 0.0)
			ratio = 1.0;
	}
}

/* Writes the radius (source pixels) of a source: 0 removes the filter, otherwise it is
 * created (last in the chain, so it sees the final image) or updated. */
void writeRadius(obs_source_t *source, double r, double ratio)
{
	obs_source_t *filter = findFilter(source);
	if (r < 0.5) {
		if (filter)
			obs_source_filter_remove(source, filter);
		return;
	}
	OBSDataAutoRelease s = obs_data_create();
	obs_data_set_double(s, "radius", r);
	obs_data_set_double(s, "ratio", ratio);
	if (filter) {
		obs_source_update(filter, s);
		return;
	}
	QString name = tr("Corners.FilterName");
	for (int i = 2;; i++) {
		OBSSourceAutoRelease taken = obs_source_get_filter_by_name(source, name.toUtf8().constData());
		if (!taken)
			break;
		name = tr("Corners.FilterName") + " " + QString::number(i);
	}
	OBSSourceAutoRelease created = obs_source_create_private(kFilterId, name.toUtf8().constData(), s);
	if (created)
		obs_source_filter_add(source, created);
}

/* Undo data: {items: [{source, radius, ratio}]}. */
void applyData(const char *json)
{
	OBSDataAutoRelease d = obs_data_create_from_json(json);
	OBSDataArrayAutoRelease items = obs_data_get_array(d, "items");
	for (size_t i = 0; i < obs_data_array_count(items); i++) {
		OBSDataAutoRelease it = obs_data_array_item(items, i);
		OBSSourceAutoRelease source = obs_get_source_by_uuid(obs_data_get_string(it, "source"));
		if (source)
			writeRadius(source, obs_data_get_double(it, "radius"), obs_data_get_double(it, "ratio"));
	}
	EditContext::instance().markDirty();
}

struct Target {
	OBSSource source;
	core::Vec2 scale;
	double canvasRadius;
};

std::vector<Target> selectedTargets()
{
	std::vector<Target> out;
	for (const ItemEntry *e : EditContext::instance().selection()) {
		obs_source_t *source = obs_sceneitem_get_source(e->item);
		if (e->isGroup || !source)
			continue;
		bool seen = false; /* a source selected twice is edited once */
		for (const Target &t : out)
			seen = seen || t.source == source;
		if (!seen)
			out.push_back({source, worldScale(*e), radius(*e)});
	}
	return out;
}

obs_data_t *snapshot(const std::vector<Target> &targets)
{
	obs_data_t *d = obs_data_create();
	OBSDataArrayAutoRelease items = obs_data_array_create();
	for (const Target &t : targets) {
		double r, ratio;
		sourceRadius(t.source, r, ratio);
		OBSDataAutoRelease it = obs_data_create();
		obs_data_set_string(it, "source", obs_source_get_uuid(t.source));
		obs_data_set_double(it, "radius", r);
		obs_data_set_double(it, "ratio", ratio);
		obs_data_array_push_back(items, it);
	}
	obs_data_set_array(d, "items", items);
	return d;
}

/* Canvas radius -> source settings, round corners on screen even with a stretched item. */
void writeCanvasRadius(const Target &t, double canvasRadius)
{
	const double sx = t.scale.x > 1e-9 ? t.scale.x : 1.0;
	const double sy = t.scale.y > 1e-9 ? t.scale.y : 1.0;
	writeRadius(t.source, std::max(0.0, canvasRadius) / sx, sx / sy);
}

void addUndo(obs_data_t *before, obs_data_t *after)
{
	if (strcmp(obs_data_get_json(before), obs_data_get_json(after)) == 0)
		return;
	const QString name = tr("Undo.Format").arg(tr("Undo.Corners"), EditContext::instance().sceneName());
	obs_frontend_add_undo_redo_action(name.toUtf8().constData(), applyData, applyData, obs_data_get_json(before),
					  obs_data_get_json(after), false);
}

struct Scrub {
	std::vector<Target> targets;
	OBSDataAutoRelease before;
	bool active = false;
};
Scrub g_scrub;

} // namespace

void registerFilter()
{
	struct obs_source_info info = {};
	info.id = kFilterId;
	info.type = OBS_SOURCE_TYPE_FILTER;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB;
	info.get_name = filterName;
	info.create = filterCreate;
	info.destroy = filterDestroy;
	info.update = filterUpdate;
	info.get_defaults = filterDefaults;
	info.get_properties = filterProperties;
	info.video_render = filterRender;
	info.video_get_color_space = filterColorSpace;
	obs_register_source(&info);
}

double radius(const ItemEntry &e)
{
	obs_source_t *source = obs_sceneitem_get_source(e.item);
	if (e.isGroup || !source)
		return 0.0;
	double r, ratio;
	sourceRadius(source, r, ratio);
	return r * worldScale(e).x;
}

bool setRadius(const core::Expression &expr)
{
	if (!expr.ok || !EditContext::instance().canEdit())
		return false;
	const std::vector<Target> targets = selectedTargets();
	if (targets.empty())
		return false;
	OBSDataAutoRelease before = snapshot(targets);
	for (const Target &t : targets) {
		double v = 0.0;
		if (expr.apply(t.canvasRadius, v))
			writeCanvasRadius(t, v);
	}
	OBSDataAutoRelease after = snapshot(targets);
	addUndo(before, after);
	EditContext::instance().markDirty();
	return true;
}

void scrub(double delta, bool finished)
{
	if (!g_scrub.active) {
		if (!EditContext::instance().canEdit())
			return;
		g_scrub.targets = selectedTargets();
		g_scrub.before = snapshot(g_scrub.targets);
		g_scrub.active = true;
	}
	for (const Target &t : g_scrub.targets)
		writeCanvasRadius(t, std::round(t.canvasRadius + delta));
	if (finished) {
		OBSDataAutoRelease after = snapshot(g_scrub.targets);
		addUndo(g_scrub.before, after);
		g_scrub = Scrub();
	}
	EditContext::instance().markDirty();
}

} // namespace layouts::corners
