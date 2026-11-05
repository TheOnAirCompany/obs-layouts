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
#include "core/shape.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <graphics/matrix4.h>
#include <util/util.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <map>
#include <mutex>
#include <string>
#include <cmath>
#include <vector>

namespace layouts::corners {

namespace {

QString tr(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

/* ---- the filter ------------------------------------------------------------- */

/* Scenes being rendered on this thread, innermost last: the scene filter pushes its
 * token while its scene renders (see SceneMarker). */
thread_local std::vector<std::string> t_scenes;

/* The shape of a source in one scene. Settings written before 0.5 have no mode: Pixels.
 * Auto and Round are resolved at render time from the visible size in canvas pixels. */
struct Radius {
	core::RadiusMode mode = core::RadiusMode::Pixels;
	float r = 0.0f;                     /* Pixels: source pixels, horizontal */
	float ratio = 1.0f;                 /* vertical radius / horizontal radius (non-uniform item scale) */
	float scaleX = 0.0f, scaleY = 0.0f; /* canvas pixels per source pixel when applied, 0: unknown */
	std::vector<core::Vec2> polygon;    /* convex, 0..1 across the visible rectangle */

	bool active() const
	{
		switch (mode) {
		case core::RadiusMode::Auto:
		case core::RadiusMode::Round:
			return true;
		case core::RadiusMode::Pixels:
			return r >= 0.5f || !polygon.empty();
		case core::RadiusMode::None:
		default:
			return !polygon.empty();
		}
	}
};

const char *modeName(core::RadiusMode m)
{
	switch (m) {
	case core::RadiusMode::Auto:
		return "auto";
	case core::RadiusMode::Round:
		return "round";
	case core::RadiusMode::None:
		return "none";
	case core::RadiusMode::Pixels:
	default:
		return "pixels";
	}
}

core::RadiusMode modeFrom(const char *name)
{
	if (name && strcmp(name, "auto") == 0)
		return core::RadiusMode::Auto;
	if (name && strcmp(name, "round") == 0)
		return core::RadiusMode::Round;
	if (name && strcmp(name, "none") == 0)
		return core::RadiusMode::None;
	return core::RadiusMode::Pixels; /* also the settings of Layouts 0.2 to 0.4 */
}

struct Filter {
	obs_source_t *context = nullptr;
	gs_effect_t *effect = nullptr;
	gs_eparam_t *size = nullptr;
	gs_eparam_t *rect = nullptr;
	gs_eparam_t *radius = nullptr;
	gs_eparam_t *polyCount = nullptr;
	gs_eparam_t *poly[4] = {};
	std::mutex lock;
	Radius any;                           /* scenes without their own value */
	std::map<std::string, Radius> scenes; /* scene token -> radius */
};

const char *filterName(void *)
{
	return obs_module_text("Corners.FilterName");
}

Radius readRadius(obs_data_t *d)
{
	Radius r;
	r.mode = modeFrom(obs_data_get_string(d, "mode"));
	r.r = (float)std::max(0.0, obs_data_get_double(d, "radius"));
	const double ratio = obs_data_get_double(d, "ratio");
	r.ratio = (float)(ratio > 0.0 ? ratio : 1.0);
	r.scaleX = (float)std::max(0.0, obs_data_get_double(d, "scale_x"));
	r.scaleY = (float)std::max(0.0, obs_data_get_double(d, "scale_y"));
	OBSDataArrayAutoRelease poly = obs_data_get_array(d, "polygon");
	std::vector<core::Vec2> pts;
	for (size_t i = 0; i < obs_data_array_count(poly); i++) {
		OBSDataAutoRelease pt = obs_data_array_item(poly, i);
		pts.push_back({obs_data_get_double(pt, "x"), obs_data_get_double(pt, "y")});
	}
	r.polygon = core::shape::convexPolygon(pts);
	return r;
}

void writeRadius(obs_data_t *d, const Radius &r)
{
	obs_data_set_string(d, "mode", modeName(r.mode));
	obs_data_set_double(d, "radius", r.r);
	obs_data_set_double(d, "ratio", r.ratio);
	if (r.scaleX > 0.0f && r.scaleY > 0.0f) {
		obs_data_set_double(d, "scale_x", r.scaleX);
		obs_data_set_double(d, "scale_y", r.scaleY);
	}
	if (!r.polygon.empty()) {
		OBSDataArrayAutoRelease poly = obs_data_array_create();
		for (const core::Vec2 &p : r.polygon) {
			OBSDataAutoRelease pt = obs_data_create();
			obs_data_set_double(pt, "x", p.x);
			obs_data_set_double(pt, "y", p.y);
			obs_data_array_push_back(poly, pt);
		}
		obs_data_set_array(d, "polygon", poly);
	}
}

/* Radius (source pixels, x and y) and polygon (source pixels) for a visible rectangle.
 * scale: canvas pixels per source pixel, from the render matrix or else as applied. */
struct Resolved {
	float rx = 0.0f, ry = 0.0f;
	std::vector<core::Vec2> polygon;
	bool visible() const { return rx >= 0.5f || ry >= 0.5f || polygon.size() >= 3; }
};

Resolved resolve(const Radius &rad, float l, float t, float r, float b, core::Vec2 scale)
{
	Resolved out;
	if (!rad.polygon.empty())
		out.polygon = core::shape::denormalize(rad.polygon, l, t, r, b);
	if (rad.mode == core::RadiusMode::Pixels) {
		out.rx = rad.r;
		out.ry = rad.r * rad.ratio;
		return out;
	}
	if (scale.x <= 1e-9 || scale.y <= 1e-9) {
		scale.x = rad.scaleX > 0.0f ? rad.scaleX : 1.0;
		scale.y = rad.scaleY > 0.0f ? rad.scaleY : scale.x;
	}
	const double canvas = core::shape::radiusFor(rad.mode, 0.0, (r - l) * scale.x, (b - t) * scale.y);
	out.rx = (float)(canvas / scale.x);
	out.ry = (float)(canvas / scale.y);
	return out;
}

Resolved blend(const Resolved &a, const Resolved &b, float t)
{
	Resolved out;
	out.rx = a.rx + (b.rx - a.rx) * t;
	out.ry = a.ry + (b.ry - a.ry) * t;
	if (a.polygon.size() == b.polygon.size()) {
		for (size_t i = 0; i < a.polygon.size(); i++)
			out.polygon.push_back({a.polygon[i].x + (b.polygon[i].x - a.polygon[i].x) * t,
					       a.polygon[i].y + (b.polygon[i].y - a.polygon[i].y) * t});
	} else {
		out.polygon = t < 0.5f ? a.polygon : b.polygon;
	}
	return out;
}

void filterUpdate(void *data, obs_data_t *settings)
{
	auto *f = static_cast<Filter *>(data);
	std::map<std::string, Radius> scenes;
	OBSDataArrayAutoRelease list = obs_data_get_array(settings, "scenes");
	for (size_t i = 0; i < obs_data_array_count(list); i++) {
		OBSDataAutoRelease it = obs_data_array_item(list, i);
		scenes[obs_data_get_string(it, "scene")] = readRadius(it);
	}
	std::lock_guard<std::mutex> g(f->lock);
	f->any = readRadius(settings);
	f->scenes = std::move(scenes);
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
		f->polyCount = gs_effect_get_param_by_name(f->effect, "polyCount");
		const char *names[] = {"poly01", "poly23", "poly45", "poly67"};
		for (int i = 0; i < 4; i++)
			f->poly[i] = gs_effect_get_param_by_name(f->effect, names[i]);
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
	obs_data_set_default_double(settings, "radius", 0.0);
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
/* scale: canvas pixels per source pixel when the matrix is the one of the scene item
 * (drawn straight into its scene, possibly rotated: lengths of the axes, not the
 * diagonal), (0, 0) when the item is drawn into its crop texture first (unknown). */
void visibleRect(float w, float h, float &left, float &top, float &right, float &bottom, core::Vec2 &scale)
{
	left = 0.0f;
	top = 0.0f;
	right = w;
	bottom = h;
	struct matrix4 m;
	gs_matrix_get(&m);
	scale = core::shape::axisScale(m.x.x, m.x.y, m.y.x, m.y.y);
	struct gs_rect vp;
	gs_get_viewport(&vp);
	if (vp.cx <= 0 || vp.cy <= 0)
		return;
	/* The crop texture: an axis aligned w/vp scale and a shift by the crop, never rotated
	 * (the rotation applies when the texture is drawn in the scene). */
	const float sx = w / (float)vp.cx, sy = h / (float)vp.cy;
	if (!core::shape::axisAligned(m.x.x, m.x.y, m.y.x, m.y.y) || !near(m.x.x, sx) || !near(m.y.y, sy) ||
	    m.t.x > 0.01f || m.t.y > 0.01f)
		return;
	scale = {0.0, 0.0};
	const float l = -m.t.x / sx, t = -m.t.y / sy;
	if (l + (float)vp.cx > w + 0.5f || t + (float)vp.cy > h + 0.5f)
		return;
	left = l;
	top = t;
	right = l + (float)vp.cx;
	bottom = t + (float)vp.cy;
}

const enum gs_color_space kSpaces[] = {GS_CS_SRGB, GS_CS_SRGB_16F, GS_CS_709_EXTENDED};

bool transitionRadius(Filter *f, obs_source_t *source, float l, float t, float r, float b, core::Vec2 scale,
		      Resolved &out);

void filterRender(void *data, gs_effect_t *)
{
	auto *f = static_cast<Filter *>(data);
	obs_source_t *target = obs_filter_get_target(f->context);
	const uint32_t w = target ? obs_source_get_width(target) : 0;
	const uint32_t h = target ? obs_source_get_height(target) : 0;
	Radius rad;
	{
		std::lock_guard<std::mutex> g(f->lock);
		rad = f->any;
		if (!t_scenes.empty()) {
			const auto it = f->scenes.find(t_scenes.back());
			if (it != f->scenes.end())
				rad = it->second;
		}
	}
	if (!f->effect || !w || !h) {
		obs_source_skip_video_filter(f->context);
		return;
	}
	/* Read before the filter renders its input (which sets its own matrix). */
	float l, t, r, b;
	core::Vec2 scale;
	visibleRect((float)w, (float)h, l, t, r, b, scale);
	Resolved shape = resolve(rad, l, t, r, b, scale);
	/* Drawn outside any scene: a transition that draws sources itself (Move). */
	if (t_scenes.empty()) {
		Resolved during;
		if (transitionRadius(f, obs_filter_get_parent(f->context), l, t, r, b, scale, during))
			shape = std::move(during);
	}
	if (!shape.visible()) {
		obs_source_skip_video_filter(f->context);
		return;
	}

	const enum gs_color_space space = obs_source_get_color_space(target, OBS_COUNTOF(kSpaces), kSpaces);
	if (!obs_source_process_filter_begin_with_color_space(f->context, gs_get_format_from_space(space), space,
							      OBS_ALLOW_DIRECT_RENDERING))
		return;
	struct vec2 size, radius;
	vec2_set(&size, (float)w, (float)h);
	vec2_set(&radius, shape.rx, shape.ry);
	struct vec4 rect;
	vec4_set(&rect, l, t, r, b);
	gs_effect_set_vec2(f->size, &size);
	gs_effect_set_vec4(f->rect, &rect);
	gs_effect_set_vec2(f->radius, &radius);
	const size_t count = shape.polygon.size() >= 3 ? std::min<size_t>(shape.polygon.size(), 8) : 0;
	if (f->polyCount)
		gs_effect_set_float(f->polyCount, (float)count);
	auto point = [&](size_t k) {
		return count ? shape.polygon[std::min(k, count - 1)] : core::Vec2{};
	};
	for (size_t i = 0; i < 4; i++) {
		if (!f->poly[i])
			continue;
		const core::Vec2 a = point(2 * i), c = point(2 * i + 1);
		struct vec4 v;
		vec4_set(&v, (float)a.x, (float)a.y, (float)c.x, (float)c.y);
		gs_effect_set_vec4(f->poly[i], &v);
	}
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

/* ---- scene marker ------------------------------------------------------------- */

/* A filter on each scene that has its own corners. While the scene renders it tells the
 * corner filters which scene they are drawn in, so a radius belongs to the scene and not
 * to the source. It renders nothing itself. Its token is copied when OBS duplicates the
 * scene (Studio Mode), so the copy on the program keeps the corners of its original. */
struct SceneMarker {
	obs_source_t *context = nullptr;
	std::mutex lock;
	std::string token;
};

const char *markerName(void *)
{
	return obs_module_text("Corners.SceneFilterName");
}

void markerUpdate(void *data, obs_data_t *settings)
{
	auto *m = static_cast<SceneMarker *>(data);
	std::lock_guard<std::mutex> g(m->lock);
	m->token = obs_data_get_string(settings, "token");
}

void *markerCreate(obs_data_t *settings, obs_source_t *context)
{
	auto *m = new SceneMarker;
	m->context = context;
	markerUpdate(m, settings);
	return m;
}

void markerDestroy(void *data)
{
	delete static_cast<SceneMarker *>(data);
}

void markerRender(void *data, gs_effect_t *)
{
	auto *m = static_cast<SceneMarker *>(data);
	{
		std::lock_guard<std::mutex> g(m->lock);
		t_scenes.push_back(m->token);
	}
	obs_source_skip_video_filter(m->context); /* the scene draws itself, as without the filter */
	t_scenes.pop_back();
}

enum gs_color_space markerColorSpace(void *data, size_t count, const enum gs_color_space *preferred)
{
	auto *m = static_cast<SceneMarker *>(data);
	obs_source_t *target = obs_filter_get_target(m->context);
	return target ? obs_source_get_color_space(target, count, preferred) : GS_CS_SRGB;
}

/* ---- transitions that draw the sources themselves ------------------------------ */

/* Token of a scene, read from its marker filter (render thread safe). */
std::string tokenOf(obs_source_t *scene)
{
	struct Find {
		std::string token;
		bool found = false;
	} f;
	obs_source_enum_filters(
		scene,
		[](obs_source_t *, obs_source_t *filter, void *p) {
			auto *f = static_cast<Find *>(p);
			if (f->found || strcmp(obs_source_get_unversioned_id(filter), kSceneFilterId) != 0)
				return;
			auto *m = static_cast<SceneMarker *>(obs_obj_get_data(filter));
			if (!m)
				return;
			std::lock_guard<std::mutex> g(m->lock);
			f->token = m->token;
			f->found = true;
		},
		&f);
	return f.token;
}

/* Radius of the source in this scene; false when the scene doesn't show it. */
bool radiusIn(Filter *f, obs_source_t *sceneSource, obs_source_t *source, Radius &out)
{
	obs_scene_t *scene = sceneSource ? obs_scene_from_source(sceneSource) : nullptr;
	if (!scene || !source || !obs_scene_find_source_recursive(scene, obs_source_get_name(source)))
		return false;
	const std::string token = tokenOf(sceneSource);
	std::lock_guard<std::mutex> g(f->lock);
	const auto it = f->scenes.find(token);
	out = it != f->scenes.end() ? it->second : f->any;
	return true;
}

float easeInOutCubic(float t)
{
	return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

/* Move Transition and the like draw each source of the two scenes themselves, never the
 * scenes: no scene marker runs. The radius then comes from the transition on the
 * program: the one of scene A, the one of scene B, or a blend of both while the source
 * moves from one to the other (eased like Move's default, cubic in and out). */
/* The transition on the program, kept from the UI thread: asking libobs for the output
 * channel while rendering would wait on the lock the render itself holds. */
std::mutex g_programLock;
OBSWeakSource g_program;

void frontendEvent(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_TRANSITION_CHANGED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		watchProgram();
		break;
	default:
		break;
	}
}

bool transitionRadius(Filter *f, obs_source_t *source, float l, float t, float r, float b, core::Vec2 scale,
		      Resolved &out)
{
	obs_source_t *raw = nullptr;
	{
		std::lock_guard<std::mutex> g(g_programLock);
		raw = g_program ? obs_weak_source_get_source(g_program) : nullptr;
	}
	OBSSourceAutoRelease tr = raw;
	if (!tr || obs_source_get_type(tr) != OBS_SOURCE_TYPE_TRANSITION || !obs_transition_is_active(tr))
		return false;
	OBSSourceAutoRelease sa = obs_transition_get_source(tr, OBS_TRANSITION_SOURCE_A);
	OBSSourceAutoRelease sb = obs_transition_get_source(tr, OBS_TRANSITION_SOURCE_B);
	Radius ra, rb;
	const bool inA = radiusIn(f, sa, source, ra);
	const bool inB = radiusIn(f, sb, source, rb);
	if (!inA && !inB)
		return false;
	if (!inA || !inB) {
		out = resolve(inA ? ra : rb, l, t, r, b, scale);
		return true;
	}
	const float time = easeInOutCubic(std::clamp(obs_transition_get_time(tr), 0.0f, 1.0f));
	out = blend(resolve(ra, l, t, r, b, scale), resolve(rb, l, t, r, b, scale), time);
	return true;
}

/* ---- editing ------------------------------------------------------------------ */

obs_source_t *findFilter(obs_source_t *source, const char *id = kFilterId)
{
	struct Find {
		const char *id;
		obs_source_t *found;
	} f{id, nullptr};
	obs_source_enum_filters(
		source,
		[](obs_source_t *, obs_source_t *filter, void *p) {
			auto *f = static_cast<Find *>(p);
			if (!f->found && strcmp(obs_source_get_unversioned_id(filter), f->id) == 0)
				f->found = filter;
		},
		&f);
	return f.found; /* weak: valid while the source keeps it */
}

/* World scale of the source pixels of an item (its own scale, bounds and group). */
core::Vec2 worldScale(const ItemEntry &e)
{
	const core::ItemGeometry g = core::computeGeometry(e.model);
	const core::Vec2 x = e.parent.mapVector({g.outputScale.x, 0.0});
	const core::Vec2 y = e.parent.mapVector({0.0, g.outputScale.y});
	return {std::hypot(x.x, x.y), std::hypot(y.x, y.y)};
}

/* ---- settings of a source's corner filter ---- */

struct Settings {
	Radius any;
	std::map<std::string, Radius> scenes;
	bool exists = false;
};

Settings readSettings(obs_source_t *source)
{
	Settings st;
	obs_source_t *filter = findFilter(source);
	if (!filter)
		return st;
	st.exists = true;
	OBSDataAutoRelease d = obs_source_get_settings(filter);
	st.any = readRadius(d);
	OBSDataArrayAutoRelease list = obs_data_get_array(d, "scenes");
	for (size_t i = 0; i < obs_data_array_count(list); i++) {
		OBSDataAutoRelease it = obs_data_array_item(list, i);
		st.scenes[obs_data_get_string(it, "scene")] = readRadius(it);
	}
	return st;
}

obs_data_t *toData(const Settings &st)
{
	obs_data_t *d = obs_data_create();
	writeRadius(d, st.any);
	OBSDataArrayAutoRelease list = obs_data_array_create();
	for (const auto &[token, r] : st.scenes) {
		OBSDataAutoRelease it = obs_data_create();
		obs_data_set_string(it, "scene", token.c_str());
		writeRadius(it, r);
		obs_data_array_push_back(list, it);
	}
	obs_data_set_array(d, "scenes", list);
	return d;
}

/* Writes the settings: no radius anywhere removes the filter, otherwise it is created
 * (last in the chain, so it sees the final image) or updated. */
void writeSettings(obs_source_t *source, Settings st)
{
	for (auto it = st.scenes.begin(); it != st.scenes.end();) {
		if (!it->second.active() && !st.any.active())
			it = st.scenes.erase(it); /* nothing where the default is nothing too */
		else
			++it;
	}
	bool any = st.any.active();
	for (const auto &[token, r] : st.scenes)
		any = any || r.active();
	obs_source_t *filter = findFilter(source);
	if (!any) {
		if (filter)
			obs_source_filter_remove(source, filter);
		return;
	}
	OBSDataAutoRelease d = toData(st);
	if (filter) {
		obs_source_update(filter, d);
		return;
	}
	QString name = tr("Corners.FilterName");
	for (int i = 2;; i++) {
		OBSSourceAutoRelease taken = obs_source_get_filter_by_name(source, name.toUtf8().constData());
		if (!taken)
			break;
		name = tr("Corners.FilterName") + " " + QString::number(i);
	}
	OBSSourceAutoRelease created = obs_source_create_private(kFilterId, name.toUtf8().constData(), d);
	if (created)
		obs_source_filter_add(source, created);
}

/* ---- scene tokens ---- */

std::string markerToken(obs_source_t *scene)
{
	obs_source_t *marker = scene ? findFilter(scene, kSceneFilterId) : nullptr;
	if (!marker)
		return {};
	OBSDataAutoRelease d = obs_source_get_settings(marker);
	return obs_data_get_string(d, "token");
}

std::vector<OBSSource> sourcesWithCorners()
{
	std::vector<OBSSource> out;
	obs_enum_all_sources(
		[](void *p, obs_source_t *s) {
			if (obs_source_get_type(s) != OBS_SOURCE_TYPE_FILTER && findFilter(s))
				static_cast<std::vector<OBSSource> *>(p)->push_back(s);
			return true;
		},
		&out);
	return out;
}

/* The token of a scene being edited, created when missing. A scene duplicated by the
 * user carries its original's token: it gets its own, keeping the corners it had. */
std::string ensureToken(obs_source_t *scene)
{
	const std::string uuid = obs_source_get_uuid(scene);
	obs_source_t *marker = findFilter(scene, kSceneFilterId);
	const std::string old = markerToken(scene);
	if (marker && old == uuid)
		return uuid;
	if (!old.empty()) {
		for (const OBSSource &s : sourcesWithCorners()) {
			Settings st = readSettings(s);
			const auto it = st.scenes.find(old);
			if (it != st.scenes.end() && !st.scenes.count(uuid)) {
				st.scenes[uuid] = it->second;
				writeSettings(s, st);
			}
		}
	}
	OBSDataAutoRelease d = obs_data_create();
	obs_data_set_string(d, "token", uuid.c_str());
	if (marker) {
		obs_source_update(marker, d);
	} else {
		OBSSourceAutoRelease created = obs_source_create_private(
			kSceneFilterId, tr("Corners.SceneFilterName").toUtf8().constData(), d);
		if (created)
			obs_source_filter_add(scene, created);
	}
	return uuid;
}

/* Corners set for every scene (Layouts 0.3, or the OBS Filters window) become values of
 * each scene that uses the source, so that editing one scene leaves the others alone. */
void splitPerScene(obs_source_t *source, Settings &st)
{
	if (!st.any.active())
		return;
	std::vector<OBSSource> scenes;
	struct Ctx {
		obs_source_t *source;
		std::vector<OBSSource> *scenes;
	} c{source, &scenes};
	obs_enum_scenes(
		[](void *p, obs_source_t *sceneSource) {
			auto *c = static_cast<Ctx *>(p);
			obs_scene_t *scene = obs_source_is_group(sceneSource) ? nullptr
									      : obs_scene_from_source(sceneSource);
			if (scene && obs_scene_find_source_recursive(scene, obs_source_get_name(c->source)))
				c->scenes->push_back(sceneSource);
			return true;
		},
		&c);
	for (const OBSSource &scene : scenes) {
		const std::string token = ensureToken(scene);
		if (!st.scenes.count(token))
			st.scenes[token] = st.any;
	}
	st.any = Radius();
}

/* ---- undo ---- */

/* Undo data: {items: [{source, settings}]}, settings "" when the source had no filter. */
void applyData(const char *json)
{
	OBSDataAutoRelease d = obs_data_create_from_json(json);
	OBSDataArrayAutoRelease items = obs_data_get_array(d, "items");
	for (size_t i = 0; i < obs_data_array_count(items); i++) {
		OBSDataAutoRelease it = obs_data_array_item(items, i);
		OBSSourceAutoRelease source = obs_get_source_by_uuid(obs_data_get_string(it, "source"));
		if (!source)
			continue;
		const char *json = obs_data_get_string(it, "settings");
		Settings st;
		if (*json) {
			OBSDataAutoRelease sd = obs_data_create_from_json(json);
			st.any = readRadius(sd);
			OBSDataArrayAutoRelease list = obs_data_get_array(sd, "scenes");
			for (size_t k = 0; k < obs_data_array_count(list); k++) {
				OBSDataAutoRelease e = obs_data_array_item(list, k);
				st.scenes[obs_data_get_string(e, "scene")] = readRadius(e);
			}
		}
		writeSettings(source, st);
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
		const Settings st = readSettings(t.source);
		OBSDataAutoRelease it = obs_data_create();
		obs_data_set_string(it, "source", obs_source_get_uuid(t.source));
		if (st.exists) {
			OBSDataAutoRelease sd = toData(st);
			obs_data_set_string(it, "settings", obs_data_get_json(sd));
		} else {
			obs_data_set_string(it, "settings", "");
		}
		obs_data_array_push_back(items, it);
	}
	obs_data_set_array(d, "items", items);
	return d;
}

/* Canvas radius -> this scene's value, round corners on screen even with a stretched item. */
void writeCanvasRadius(const Target &t, const std::string &token, double canvasRadius)
{
	const double sx = t.scale.x > 1e-9 ? t.scale.x : 1.0;
	const double sy = t.scale.y > 1e-9 ? t.scale.y : 1.0;
	Settings st = readSettings(t.source);
	splitPerScene(t.source, st);
	auto it = st.scenes.find(token);
	Radius r = it != st.scenes.end() ? it->second : st.any;
	r.mode = core::RadiusMode::Pixels;
	r.r = (float)(std::max(0.0, canvasRadius) / sx);
	r.ratio = (float)(sx / sy);
	r.scaleX = (float)sx;
	r.scaleY = (float)sy;
	st.scenes[token] = r;
	writeSettings(t.source, st);
}

void addUndo(obs_data_t *before, obs_data_t *after, const QString &sceneName)
{
	if (strcmp(obs_data_get_json(before), obs_data_get_json(after)) == 0)
		return;
	const QString name = tr("Undo.Format").arg(tr("Undo.Corners"), sceneName);
	obs_frontend_add_undo_redo_action(name.toUtf8().constData(), applyData, applyData, obs_data_get_json(before),
					  obs_data_get_json(after), false);
}

/* The scene that renders an item (and so its scene marker): its own scene, or for an
 * item of a group the scene that holds the group. */
obs_source_t *topScene(obs_sceneitem_t *item)
{
	obs_scene_t *scene = obs_sceneitem_get_scene(item);
	obs_source_t *source = scene ? obs_scene_get_source(scene) : nullptr;
	if (!source || !obs_source_is_group(source))
		return source;
	struct Find {
		obs_source_t *group;
		obs_source_t *found = nullptr;
	} f{source};
	obs_enum_scenes(
		[](void *p, obs_source_t *s) {
			auto *f = static_cast<Find *>(p);
			obs_scene_t *sc = obs_source_is_group(s) ? nullptr : obs_scene_from_source(s);
			if (sc && obs_scene_find_source(sc, obs_source_get_name(f->group))) {
				f->found = s;
				return false;
			}
			return true;
		},
		&f);
	return f.found; /* weak: the scene list keeps it */
}

/* Canvas pixels per source pixel of an item, rotation aside (and its group's). */
core::Vec2 itemScale(obs_sceneitem_t *item)
{
	struct matrix4 m;
	obs_sceneitem_get_draw_transform(item, &m);
	core::Vec2 s = core::shape::axisScale(m.x.x, m.x.y, m.y.x, m.y.y);
	obs_scene_t *scene = obs_sceneitem_get_scene(item);
	obs_source_t *sceneSource = scene ? obs_scene_get_source(scene) : nullptr;
	obs_source_t *top = sceneSource && obs_source_is_group(sceneSource) ? topScene(item) : nullptr;
	obs_scene_t *topSc = top ? obs_scene_from_source(top) : nullptr;
	obs_sceneitem_t *group = topSc ? obs_scene_find_source(topSc, obs_source_get_name(sceneSource)) : nullptr;
	if (group) {
		obs_sceneitem_get_draw_transform(group, &m);
		const core::Vec2 g = core::shape::axisScale(m.x.x, m.x.y, m.y.x, m.y.y);
		s = {s.x * g.x, s.y * g.y};
	}
	if (s.x <= 1e-9)
		s.x = 1.0;
	if (s.y <= 1e-9)
		s.y = 1.0;
	return s;
}

struct Scrub {
	std::vector<Target> targets;
	std::string token;
	OBSDataAutoRelease before;
	bool active = false;
};
Scrub g_scrub;

} // namespace

void watchProgram()
{
	OBSSourceAutoRelease tr = obs_get_output_source(0);
	OBSWeakSourceAutoRelease weak = tr ? obs_source_get_weak_source(tr) : nullptr;
	std::lock_guard<std::mutex> g(g_programLock);
	g_program = weak.Get();
}

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

	struct obs_source_info marker = {};
	marker.id = kSceneFilterId;
	marker.type = OBS_SOURCE_TYPE_FILTER;
	marker.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB | OBS_SOURCE_CAP_DISABLED;
	marker.get_name = markerName;
	marker.create = markerCreate;
	marker.destroy = markerDestroy;
	marker.update = markerUpdate;
	marker.video_render = markerRender;
	marker.video_get_color_space = markerColorSpace;
	obs_register_source(&marker);
	obs_frontend_add_event_callback(frontendEvent, nullptr);
}

double radius(const ItemEntry &e)
{
	obs_source_t *source = obs_sceneitem_get_source(e.item);
	if (e.isGroup || !source)
		return 0.0;
	const Settings st = readSettings(source);
	Radius r = st.any;
	const auto it = st.scenes.find(markerToken(EditContext::instance().sceneSource()));
	if (it != st.scenes.end())
		r = it->second;
	const core::Vec2 scale = worldScale(e);
	if (r.mode == core::RadiusMode::Pixels)
		return r.r * scale.x;
	const core::ItemGeometry g = core::computeGeometry(e.model);
	return core::shape::radiusFor(r.mode, 0.0, g.croppedWidth * scale.x, g.croppedHeight * scale.y);
}

std::vector<core::Vec2> itemPolygon(obs_sceneitem_t *item)
{
	std::vector<core::Vec2> out;
	if (!item)
		return out;
	OBSDataAutoRelease priv = obs_sceneitem_get_private_settings(item);
	QByteArray json = QByteArray(obs_data_get_string(priv, "layouts_style"));
	if (json.isEmpty()) {
		OBSDataAutoRelease obj = obs_data_get_obj(priv, "layouts_style");
		if (obj)
			json = QByteArray(obs_data_get_json(obj));
	}
	const QJsonArray points = QJsonDocument::fromJson(json).object().value("polygon").toArray();
	for (const QJsonValue v : points) {
		const QJsonArray pt = v.toArray();
		if (pt.size() >= 2)
			out.push_back({pt.at(0).toDouble(), pt.at(1).toDouble()});
	}
	return core::shape::convexPolygon(out);
}

bool applyItemStyle(obs_sceneitem_t *item, const core::Style &style, const std::vector<core::Vec2> *polygon, bool undo)
{
	obs_source_t *source = item ? obs_sceneitem_get_source(item) : nullptr;
	if (!source || obs_source_is_group(source) || obs_source_is_scene(source))
		return false;
	obs_source_t *sceneSource = topScene(item);
	if (!sceneSource)
		return false;
	const core::Vec2 scale = itemScale(item);
	const std::vector<Target> targets{{OBSSource(source), scale, 0.0}};
	OBSDataAutoRelease before = snapshot(targets);
	const std::string token = ensureToken(sceneSource);

	Radius r;
	r.mode = style.radiusMode;
	r.scaleX = (float)scale.x;
	r.scaleY = (float)scale.y;
	r.ratio = (float)(scale.x / scale.y);
	r.r = r.mode == core::RadiusMode::Pixels ? (float)(std::max(0.0, style.radiusPx) / scale.x) : 0.0f;
	r.polygon = polygon ? core::shape::convexPolygon(*polygon) : itemPolygon(item);

	Settings st = readSettings(source);
	splitPerScene(source, st);
	st.scenes[token] = r;
	writeSettings(source, st);

	OBSDataAutoRelease after = snapshot(targets);
	if (undo)
		addUndo(before, after, QString::fromUtf8(obs_source_get_name(sceneSource)));
	EditContext::instance().markDirty();
	return true;
}

bool setRadius(const core::Expression &expr)
{
	EditContext &ctx = EditContext::instance();
	if (!expr.ok || !ctx.canEdit() || !ctx.sceneSource())
		return false;
	const std::vector<Target> targets = selectedTargets();
	if (targets.empty())
		return false;
	OBSDataAutoRelease before = snapshot(targets);
	const std::string token = ensureToken(ctx.sceneSource());
	for (const Target &t : targets) {
		double v = 0.0;
		if (expr.apply(t.canvasRadius, v))
			writeCanvasRadius(t, token, v);
	}
	OBSDataAutoRelease after = snapshot(targets);
	addUndo(before, after, ctx.sceneName());
	ctx.markDirty();
	return true;
}

void scrub(double delta, bool finished)
{
	EditContext &ctx = EditContext::instance();
	if (!g_scrub.active) {
		if (!ctx.canEdit() || !ctx.sceneSource())
			return;
		g_scrub.targets = selectedTargets();
		g_scrub.before = snapshot(g_scrub.targets);
		g_scrub.token = ensureToken(ctx.sceneSource());
		g_scrub.active = true;
	}
	for (const Target &t : g_scrub.targets)
		writeCanvasRadius(t, g_scrub.token, std::round(t.canvasRadius + delta));
	if (finished) {
		OBSDataAutoRelease after = snapshot(g_scrub.targets);
		addUndo(g_scrub.before, after, ctx.sceneName());
		g_scrub = Scrub();
	}
	ctx.markDirty();
}

/* ---- shared with the border and shadow filter (style-render.cpp) ---------------- */

std::string renderingScene()
{
	return t_scenes.empty() ? std::string() : t_scenes.back();
}

std::string sceneToken(obs_source_t *scene)
{
	return scene ? ensureToken(scene) : std::string();
}

std::string sceneTokenAtRender(obs_source_t *scene)
{
	return scene ? tokenOf(scene) : std::string();
}

OBSSource programTransition()
{
	obs_source_t *raw = nullptr;
	{
		std::lock_guard<std::mutex> g(g_programLock);
		raw = g_program ? obs_weak_source_get_source(g_program) : nullptr;
	}
	OBSSourceAutoRelease tr = raw;
	return tr.Get();
}

bool renderRadius(obs_source_t *source, float &r, float &ratio)
{
	obs_source_t *filter = source ? findFilter(source) : nullptr;
	auto *f = filter && obs_source_enabled(filter) ? static_cast<Filter *>(obs_obj_get_data(filter)) : nullptr;
	if (!f)
		return false;
	Radius rad;
	{
		std::lock_guard<std::mutex> g(f->lock);
		rad = f->any;
		if (!t_scenes.empty()) {
			const auto it = f->scenes.find(t_scenes.back());
			if (it != f->scenes.end())
				rad = it->second;
		}
	}
	if (t_scenes.empty()) {
		Radius during;
		if (transitionRadius(f, source, during))
			rad = during;
	}
	r = rad.r;
	ratio = rad.ratio;
	return true;
}

} // namespace layouts::corners
