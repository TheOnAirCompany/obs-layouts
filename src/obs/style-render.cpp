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

/* Border and drop shadow.
 *
 * Where the shadow is drawn (the hard part: a filter is clipped to its source size when
 * OBS renders it into a texture). Options studied:
 *
 *  1. A filter that reports a larger width/height and draws the source shifted inside.
 *     Rejected: the item's size changes, so OBS's crop, bounds fitting ("scale to inner
 *     bounds" shrinks the picture), alignment and the Inspector's numbers all change; the
 *     compensation (moving the item, rewriting crop and bounds) fights the user and every
 *     other plugin that reads the transform.
 *  2. The scene marker drawing every shadow before its scene. Rejected: the shadows end up
 *     under all items, a full-screen background hides them all.
 *  3. A companion scene item under each item, kept in sync. Robust with crop, but visible
 *     in the user's sources and has to follow every transform change: heavy.
 *  4. (chosen) The filter draws the shadow itself, in the item's own space, just before
 *     the item and after the items below it: right stacking order, follows position,
 *     scale, rotation, bounds and groups, and Move Transition (which draws the sources
 *     with its own matrix) for free. Cost: one quad, no texture, no blur pass (signed
 *     distance to the rounded rectangle, smoothstep over the blur width).
 *     An item that OBS renders through its own texture (crop, bounds crop, a non-default
 *     scale filter, sRGB-off blending) is clipped to its visible part: its shadow is drawn
 *     in the scene instead, just before the item (drawShadowInScene). Items inside a group
 *     keep no shadow in that case, and a second view of the same scene in the same frame
 *     (projector, multiview) reuses the item texture without calling its filters: no
 *     shadow there; the program output (first render of the frame) has it.
 *
 * The border is an inner stroke drawn over the item as a second quad (no texture either),
 * so the chain never goes through an extra texture: the corners filter keeps seeing the
 * real crop matrix. */

#include "style-render.hpp"

#include "corners.hpp"
#include "edit-context.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <graphics/matrix4.h>
#include <graphics/vec4.h>
#include <util/util.hpp>

#include <QString>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <mutex>
#include <string>

namespace layouts::obs::style {

namespace {

QString tr(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

/* Values of one scene. Lengths in canvas pixels, sx/sy = canvas pixels per source pixel
 * of the item when they were set. */
struct Look {
	bool border = false;
	float width = 4.0f;
	uint32_t borderColor = 0xFFFFFFFF; /* ARGB */
	int preset = (int)core::ShadowPreset::None;
	float blur = 24.0f, x = 0.0f, y = 8.0f, opacity = 0.35f;
	uint32_t shadowColor = 0xFF000000; /* ARGB */
	bool fade = true;
	float sx = 1.0f, sy = 1.0f;

	bool shadow() const { return preset != (int)core::ShadowPreset::None && opacity > 0.0f; }
	bool empty() const { return !border && !shadow(); }
};

Look readLook(obs_data_t *d)
{
	Look l;
	l.border = obs_data_get_bool(d, "border");
	l.width = (float)std::max(0.0, obs_data_get_double(d, "width"));
	l.borderColor = (uint32_t)obs_data_get_int(d, "borderColor");
	l.preset = (int)obs_data_get_int(d, "preset");
	l.blur = (float)std::max(0.0, obs_data_get_double(d, "blur"));
	l.x = (float)obs_data_get_double(d, "x");
	l.y = (float)obs_data_get_double(d, "y");
	l.opacity = (float)std::clamp(obs_data_get_double(d, "opacity"), 0.0, 1.0);
	l.shadowColor = (uint32_t)obs_data_get_int(d, "shadowColor");
	l.fade = obs_data_get_bool(d, "fade");
	const double sx = obs_data_get_double(d, "sx"), sy = obs_data_get_double(d, "sy");
	l.sx = (float)(sx > 1e-6 ? sx : 1.0);
	l.sy = (float)(sy > 1e-6 ? sy : 1.0);
	return l;
}

void writeLook(obs_data_t *d, const Look &l)
{
	obs_data_set_bool(d, "border", l.border);
	obs_data_set_double(d, "width", l.width);
	obs_data_set_int(d, "borderColor", l.borderColor);
	obs_data_set_int(d, "preset", l.preset);
	obs_data_set_double(d, "blur", l.blur);
	obs_data_set_double(d, "x", l.x);
	obs_data_set_double(d, "y", l.y);
	obs_data_set_double(d, "opacity", l.opacity);
	obs_data_set_int(d, "shadowColor", l.shadowColor);
	obs_data_set_bool(d, "fade", l.fade);
	obs_data_set_double(d, "sx", l.sx);
	obs_data_set_double(d, "sy", l.sy);
}

using Looks = std::map<std::string, Look>; /* scene token -> values */

Looks readLooks(obs_data_t *settings)
{
	Looks out;
	OBSDataArrayAutoRelease list = obs_data_get_array(settings, "scenes");
	for (size_t i = 0; i < obs_data_array_count(list); i++) {
		OBSDataAutoRelease it = obs_data_array_item(list, i);
		out[obs_data_get_string(it, "scene")] = readLook(it);
	}
	return out;
}

obs_data_t *toData(const Looks &looks)
{
	obs_data_t *d = obs_data_create();
	OBSDataArrayAutoRelease list = obs_data_array_create();
	for (const auto &[token, l] : looks) {
		OBSDataAutoRelease it = obs_data_create();
		obs_data_set_string(it, "scene", token.c_str());
		writeLook(it, l);
		obs_data_array_push_back(list, it);
	}
	obs_data_set_array(d, "scenes", list);
	return d;
}

/* ---- the filter ------------------------------------------------------------- */

struct Filter {
	obs_source_t *context = nullptr;
	gs_effect_t *effect = nullptr;
	gs_eparam_t *quad = nullptr, *aspect = nullptr, *rect = nullptr, *radius = nullptr;
	gs_eparam_t *color = nullptr, *width = nullptr, *offset = nullptr, *blur = nullptr;
	std::mutex lock;
	Looks looks;
};

const char *filterName(void *)
{
	return obs_module_text("BorderShadow.FilterName");
}

void filterUpdate(void *data, obs_data_t *settings)
{
	auto *f = static_cast<Filter *>(data);
	Looks looks = readLooks(settings);
	std::lock_guard<std::mutex> g(f->lock);
	f->looks = std::move(looks);
}

void *filterCreate(obs_data_t *settings, obs_source_t *context)
{
	auto *f = new Filter;
	f->context = context;
	BPtr<char> path = obs_module_file("effects/border-shadow.effect");
	obs_enter_graphics();
	f->effect = path ? gs_effect_create_from_file(path, nullptr) : nullptr;
	if (f->effect) {
		f->quad = gs_effect_get_param_by_name(f->effect, "quad");
		f->aspect = gs_effect_get_param_by_name(f->effect, "aspect");
		f->rect = gs_effect_get_param_by_name(f->effect, "rect");
		f->radius = gs_effect_get_param_by_name(f->effect, "radius");
		f->color = gs_effect_get_param_by_name(f->effect, "color");
		f->width = gs_effect_get_param_by_name(f->effect, "width");
		f->offset = gs_effect_get_param_by_name(f->effect, "offset");
		f->blur = gs_effect_get_param_by_name(f->effect, "blur");
	}
	obs_leave_graphics();
	if (!f->effect)
		blog(LOG_WARNING, "[layouts] border and shadow effect could not be loaded");
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

bool near(float a, float b)
{
	return std::fabs(a - b) <= 1e-3f * std::max(1.0f, std::fabs(b));
}

/* Visible rect of the item being drawn (source pixels), as in corners.cpp. Returns true
 * when OBS draws the item through its own texture (crop and the like): nothing can be
 * drawn outside the visible part then. */
bool visibleRect(float w, float h, float &left, float &top, float &right, float &bottom)
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
		return false;
	const float sx = w / (float)vp.cx, sy = h / (float)vp.cy;
	if (!near(m.x.x, sx) || !near(m.y.y, sy) || std::fabs(m.x.y) > 1e-4f || std::fabs(m.y.x) > 1e-4f ||
	    m.t.x > 0.01f || m.t.y > 0.01f)
		return false;
	const float l = -m.t.x / sx, t = -m.t.y / sy;
	if (l + (float)vp.cx > w + 0.5f || t + (float)vp.cy > h + 0.5f)
		return false;
	left = l;
	top = t;
	right = l + (float)vp.cx;
	bottom = t + (float)vp.cy;
	return true;
}

float easeInOutCubic(float t)
{
	return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

bool shows(obs_source_t *sceneSource, obs_source_t *source)
{
	obs_scene_t *scene = sceneSource ? obs_scene_from_source(sceneSource) : nullptr;
	return scene && source && obs_scene_find_source_recursive(scene, obs_source_get_name(source));
}

/* Values for the render in progress and the shadow fade of a program transition.
 *
 * In a scene: that scene's values. Outside any scene (Move Transition draws the sources
 * itself): the values of scene A, then of scene B from the middle of the transition on.
 * While the program transition shows the source on either side, the shadow fades out and
 * back in (1 -> 0 at the middle -> 1, cubic in and out), with the same cached transition
 * as the corners (never the output channel from the render thread). */
bool resolve(Filter *f, obs_source_t *source, Look &out, float &fade)
{
	fade = 1.0f;
	const std::string token = corners::renderingScene();
	OBSSource tr = corners::programTransition();
	const bool active = tr && obs_source_get_type(tr) == OBS_SOURCE_TYPE_TRANSITION && obs_transition_is_active(tr);
	OBSSourceAutoRelease a = active ? obs_transition_get_source(tr, OBS_TRANSITION_SOURCE_A) : nullptr;
	OBSSourceAutoRelease b = active ? obs_transition_get_source(tr, OBS_TRANSITION_SOURCE_B) : nullptr;
	const bool inA = active && shows(a, source), inB = active && shows(b, source);
	const float t = active ? std::clamp(obs_transition_get_time(tr), 0.0f, 1.0f) : 0.0f;

	bool found = false;
	{
		std::lock_guard<std::mutex> g(f->lock);
		auto find = [&](const std::string &key) {
			const auto it = f->looks.find(key);
			if (it == f->looks.end())
				return false;
			out = it->second;
			return true;
		};
		if (!token.empty()) {
			found = find(token);
		} else if (inA || inB) {
			const std::string ta = inA ? corners::sceneTokenAtRender(a) : std::string();
			const std::string tb = inB ? corners::sceneTokenAtRender(b) : std::string();
			if (t >= 0.5f)
				found = (inB && find(tb)) || (inA && find(ta));
			else
				found = (inA && find(ta)) || (inB && find(tb));
		}
	}
	if (found && (inA || inB) && out.fade)
		fade = 1.0f - easeInOutCubic(1.0f - std::fabs(2.0f * t - 1.0f));
	return found;
}

/* ARGB -> premultiplied color in the space being rendered. */
void premultiplied(uint32_t argb, float alpha, struct vec4 &out)
{
	const uint32_t abgr = (argb & 0xFF00FF00u) | ((argb >> 16) & 0xFFu) | ((argb & 0xFFu) << 16);
	if (gs_get_linear_srgb())
		vec4_from_rgba_srgb(&out, abgr);
	else
		vec4_from_rgba(&out, abgr);
	const float a = std::clamp(out.w * alpha, 0.0f, 1.0f);
	vec4_set(&out, out.x * a, out.y * a, out.z * a, a);
}

/* Draws one technique over quad (source pixels). */
void drawQuad(Filter *f, const char *technique, float l, float t, float r, float b)
{
	struct vec4 quad;
	vec4_set(&quad, l, t, r, b);
	gs_effect_set_vec4(f->quad, &quad);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	gs_matrix_push();
	gs_matrix_translate3f(l, t, 0.0f);
	gs_matrix_scale3f(r - l, b - t, 1.0f);
	while (gs_effect_loop(f->effect, technique))
		gs_draw_sprite(nullptr, 0, 1, 1);
	gs_matrix_pop();
	gs_blend_state_pop();
}

/* An item that OBS draws through its own texture (crop): that texture is cut to the
 * visible part, so the shadow is drawn in the scene instead, just before the item is
 * drawn there (right stacking). The scene marker recorded where the scene draws (target,
 * viewport, matrix); the scene projection is the one texrender_begin saved on top of the
 * projection stack, popped for the draw and pushed back after, so it is right in every
 * view (program, preview, projectors, multiview). Items inside a group are skipped. */
void drawShadowInScene(Filter *f, obs_source_t *source, const Look &look, float opacity, float l, float t, float r,
		       float b, uint32_t w, uint32_t h)
{
	corners::SceneRenderContext ctx;
	if (!corners::sceneRenderContext(ctx) || !ctx.scene)
		return;
	obs_scene_t *scene = obs_scene_from_source(ctx.scene);
	obs_sceneitem_t *item = scene ? obs_scene_find_source(scene, obs_source_get_name(source)) : nullptr;
	if (!item)
		return;
	struct matrix4 draw;
	obs_sceneitem_get_draw_transform(item, &draw);

	gs_texture_t *itemTarget = gs_get_render_target();
	gs_zstencil_t *itemZs = gs_get_zstencil_target();
	struct gs_rect itemVp;
	gs_get_viewport(&itemVp);
	const enum gs_color_space itemSpace = gs_get_color_space();

	gs_projection_pop(); /* the scene projection, saved by gs_texrender_begin */
	gs_set_render_target_with_color_space(ctx.target, ctx.zstencil, ctx.space);
	gs_set_viewport(ctx.viewport.x, ctx.viewport.y, ctx.viewport.cx, ctx.viewport.cy);
	gs_matrix_push();
	gs_matrix_set(&ctx.matrix);
	gs_matrix_mul(&draw);
	const bool previous = gs_set_linear_srgb(true);

	/* The texture holds the visible part: the box is (0, 0, r - l, b - t). */
	const float bw = r - l, bh = b - t;
	struct vec4 box;
	vec4_set(&box, 0.0f, 0.0f, bw, bh);
	gs_effect_set_vec4(f->rect, &box);
	struct matrix4 m;
	gs_matrix_get(&m);
	const float angle = std::atan2(m.x.y, m.x.x);
	const float c = std::cos(-angle), s = std::sin(-angle);
	struct vec2 offset;
	vec2_set(&offset, look.x * c - look.y * s, look.x * s + look.y * c);
	const float pad = look.blur + std::max(std::fabs(offset.x), std::fabs(offset.y)) + 2.0f;
	struct vec4 color;
	premultiplied(look.shadowColor, opacity, color);
	gs_effect_set_vec4(f->color, &color);
	gs_effect_set_vec2(f->offset, &offset);
	gs_effect_set_float(f->blur, look.blur);
	drawQuad(f, "Shadow", -pad / look.sx, -pad / look.sy, bw + pad / look.sx, bh + pad / look.sy);

	gs_set_linear_srgb(previous);
	gs_matrix_pop();
	gs_set_render_target_with_color_space(itemTarget, itemZs, itemSpace);
	gs_set_viewport(itemVp.x, itemVp.y, itemVp.cx, itemVp.cy);
	gs_projection_push();
	gs_ortho(0.0f, (float)w, 0.0f, (float)h, -100.0f, 100.0f); /* as render_item set it */
}

void filterRender(void *data, gs_effect_t *)
{
	auto *f = static_cast<Filter *>(data);
	obs_source_t *target = obs_filter_get_target(f->context);
	obs_source_t *parent = obs_filter_get_parent(f->context);
	const uint32_t w = target ? obs_source_get_width(target) : 0;
	const uint32_t h = target ? obs_source_get_height(target) : 0;
	Look look;
	float fade = 1.0f;
	if (!f->effect || !w || !h || !resolve(f, parent, look, fade) || look.empty()) {
		obs_source_skip_video_filter(f->context);
		return;
	}

	/* Read before the item draws (its filters set their own matrix). */
	float l, t, r, b;
	const bool itemTexture = visibleRect((float)w, (float)h, l, t, r, b);
	struct matrix4 m;
	gs_matrix_get(&m);

	float cornerR = 0.0f, ratio = 1.0f;
	corners::renderRadius(parent, cornerR, ratio);
	struct vec2 aspect;
	vec2_set(&aspect, look.sx, look.sy);
	struct vec4 rect;
	vec4_set(&rect, l, t, r, b);
	gs_effect_set_vec2(f->aspect, &aspect);
	gs_effect_set_vec4(f->rect, &rect);
	gs_effect_set_float(f->radius, cornerR * look.sx);

	const float opacity = look.opacity * fade;
	if (look.shadow() && opacity > 0.001f && !itemTexture) {
		/* The offset goes down the canvas whatever the item's rotation. */
		const float angle = std::atan2(m.x.y, m.x.x);
		const float c = std::cos(-angle), s = std::sin(-angle);
		struct vec2 offset;
		vec2_set(&offset, look.x * c - look.y * s, look.x * s + look.y * c);
		const float pad = look.blur + std::max(std::fabs(offset.x), std::fabs(offset.y)) + 2.0f;
		struct vec4 color;
		premultiplied(look.shadowColor, opacity, color);
		gs_effect_set_vec4(f->color, &color);
		gs_effect_set_vec2(f->offset, &offset);
		gs_effect_set_float(f->blur, look.blur);
		drawQuad(f, "Shadow", l - pad / look.sx, t - pad / look.sy, r + pad / look.sx, b + pad / look.sy);
	} else if (look.shadow() && opacity > 0.001f && itemTexture) {
		drawShadowInScene(f, parent, look, opacity, l, t, r, b, w, h);
		/* Back to the item's own rectangle for the border. */
		gs_effect_set_vec4(f->rect, &rect);
	}

	obs_source_skip_video_filter(f->context);

	if (look.border && look.width > 0.0f) {
		struct vec4 color;
		premultiplied(look.borderColor, 1.0f, color);
		gs_effect_set_vec4(f->color, &color);
		gs_effect_set_float(f->width, look.width);
		drawQuad(f, "Border", l, t, r, b);
	}
}

const enum gs_color_space kSpaces[] = {GS_CS_SRGB, GS_CS_SRGB_16F, GS_CS_709_EXTENDED};

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
			auto **found = static_cast<obs_source_t **>(p);
			if (!*found && strcmp(obs_source_get_unversioned_id(filter), kFilterId) == 0)
				*found = filter;
		},
		&found);
	return found; /* weak: valid while the source keeps it */
}

Looks readSource(obs_source_t *source, bool *exists = nullptr)
{
	obs_source_t *filter = findFilter(source);
	if (exists)
		*exists = filter != nullptr;
	if (!filter)
		return {};
	OBSDataAutoRelease d = obs_source_get_settings(filter);
	return readLooks(d);
}

/* No value left removes the filter; otherwise it is created or updated, and kept last so
 * that it draws around the final (rounded) picture. */
void writeSource(obs_source_t *source, const Looks &looks)
{
	obs_source_t *filter = findFilter(source);
	if (looks.empty()) {
		if (filter)
			obs_source_filter_remove(source, filter);
		return;
	}
	OBSDataAutoRelease d = toData(looks);
	if (filter) {
		obs_source_update(filter, d);
		obs_source_filter_set_order(source, filter, OBS_ORDER_MOVE_BOTTOM);
		return;
	}
	QString name = tr("BorderShadow.FilterName");
	for (int i = 2;; i++) {
		OBSSourceAutoRelease taken = obs_source_get_filter_by_name(source, name.toUtf8().constData());
		if (!taken)
			break;
		name = tr("BorderShadow.FilterName") + " " + QString::number(i);
	}
	OBSSourceAutoRelease created = obs_source_create_private(kFilterId, name.toUtf8().constData(), d);
	if (created)
		obs_source_filter_add(source, created);
}

/* Undo data: {source, settings}, settings "" when the source had no filter. */
obs_data_t *snapshot(obs_source_t *source)
{
	obs_data_t *d = obs_data_create();
	obs_data_set_string(d, "source", obs_source_get_uuid(source));
	bool exists = false;
	const Looks looks = readSource(source, &exists);
	if (exists) {
		OBSDataAutoRelease sd = toData(looks);
		obs_data_set_string(d, "settings", obs_data_get_json(sd));
	} else {
		obs_data_set_string(d, "settings", "");
	}
	return d;
}

void applyData(const char *json)
{
	OBSDataAutoRelease d = obs_data_create_from_json(json);
	OBSSourceAutoRelease source = obs_get_source_by_uuid(obs_data_get_string(d, "source"));
	if (!source)
		return;
	const char *settings = obs_data_get_string(d, "settings");
	Looks looks;
	if (*settings) {
		OBSDataAutoRelease sd = obs_data_create_from_json(settings);
		looks = readLooks(sd);
	}
	writeSource(source, looks);
	EditContext::instance().markDirty();
}

/* The scene whose values an item uses: its own, or the edited scene for an item inside a
 * group (groups have no scene marker of their own). */
obs_source_t *sceneOf(obs_sceneitem_t *item)
{
	obs_scene_t *scene = obs_sceneitem_get_scene(item);
	obs_source_t *source = scene ? obs_scene_get_source(scene) : nullptr;
	if (source && obs_source_is_group(source))
		source = EditContext::instance().sceneSource();
	return source;
}

} // namespace

void shadowValues(const core::Style &s, double &blur, double &x, double &y, double &opacity)
{
	switch (s.shadow) {
	case core::ShadowPreset::None:
		blur = 0;
		x = 0;
		y = 0;
		opacity = 0;
		break;
	case core::ShadowPreset::Soft:
		blur = 16;
		x = 0;
		y = 6;
		opacity = 0.25;
		break;
	case core::ShadowPreset::Hard:
		blur = 6;
		x = 0;
		y = 6;
		opacity = 0.6;
		break;
	case core::ShadowPreset::Custom:
		blur = std::max(0.0, s.shadowBlur);
		x = s.shadowOffsetX;
		y = s.shadowOffsetY;
		opacity = std::clamp(s.shadowOpacity, 0.0, 1.0);
		break;
	}
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
	info.video_render = filterRender;
	info.video_get_color_space = filterColorSpace;
	obs_register_source(&info);
}

bool setBorderShadow(obs_sceneitem_t *item, const core::Style &style, bool undo)
{
	if (!item || obs_sceneitem_is_group(item))
		return false;
	obs_source_t *source = obs_sceneitem_get_source(item);
	obs_source_t *scene = sceneOf(item);
	if (!source || !scene)
		return false;
	const std::string token = corners::sceneToken(scene);
	if (token.empty())
		return false;

	struct matrix4 m;
	obs_sceneitem_get_draw_transform(item, &m);
	Look l;
	l.border = style.border && style.borderWidth > 0.0;
	l.width = (float)std::max(0.0, style.borderWidth);
	l.borderColor = style.borderColor;
	l.preset = (int)style.shadow;
	double blur, x, y, opacity;
	shadowValues(style, blur, x, y, opacity);
	l.blur = (float)blur;
	l.x = (float)x;
	l.y = (float)y;
	l.opacity = (float)opacity;
	l.shadowColor = style.shadowColor;
	l.fade = style.fadeShadowInTransition;
	const float sx = std::hypot(m.x.x, m.x.y), sy = std::hypot(m.y.x, m.y.y);
	l.sx = sx > 1e-6f ? sx : 1.0f;
	l.sy = sy > 1e-6f ? sy : 1.0f;

	OBSDataAutoRelease before = snapshot(source);
	Looks looks = readSource(source);
	if (l.empty())
		looks.erase(token);
	else
		looks[token] = l;
	writeSource(source, looks);
	OBSDataAutoRelease after = snapshot(source);

	if (undo && strcmp(obs_data_get_json(before), obs_data_get_json(after)) != 0) {
		const QString name =
			tr("Undo.Format").arg(tr("Undo.BorderShadow"), QString::fromUtf8(obs_source_get_name(scene)));
		obs_frontend_add_undo_redo_action(name.toUtf8().constData(), applyData, applyData,
						  obs_data_get_json(before), obs_data_get_json(after), false);
	}
	EditContext::instance().markDirty();
	return true;
}

bool borderShadow(obs_sceneitem_t *item, core::Style &out)
{
	obs_source_t *source = item ? obs_sceneitem_get_source(item) : nullptr;
	obs_source_t *scene = item ? sceneOf(item) : nullptr;
	if (!source || !scene)
		return false;
	const Looks looks = readSource(source);
	const auto it = looks.find(corners::sceneTokenAtRender(scene));
	if (it == looks.end())
		return false;
	const Look &l = it->second;
	out.border = l.border;
	out.borderWidth = l.width;
	out.borderColor = l.borderColor;
	out.shadow = (core::ShadowPreset)l.preset;
	out.shadowBlur = l.blur;
	out.shadowOffsetX = l.x;
	out.shadowOffsetY = l.y;
	out.shadowOpacity = l.opacity;
	out.shadowColor = l.shadowColor;
	out.fadeShadowInTransition = l.fade;
	return true;
}

} // namespace layouts::obs::style
