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

#include "live.hpp"

#include "core/live.hpp"

#include <obs-frontend-api.h>

#include <cstring>
#include <vector>

namespace layouts::live {

namespace {

/* Vertical for OBS's virtual camera (obs-vertical, src/obs/virtualcam.hpp). */
constexpr const char *kVerticalVirtualCam = "vertical_virtualcam";

OBSSource activeOf(obs_source_t *src)
{
	if (src && obs_source_get_type(src) == OBS_SOURCE_TYPE_TRANSITION) {
		OBSSourceAutoRelease active = obs_transition_get_active_source(src);
		return OBSSource(active.Get());
	}
	return OBSSource(src);
}

bool isMain(obs_canvas_t *canvas)
{
	obs_canvas_t *main = obs_get_main_canvas();
	const bool same = canvas == main;
	obs_canvas_release(main);
	return same;
}

bool nests(obs_source_t *program, obs_source_t *scene)
{
	struct Find {
		obs_source_t *scene;
		bool found;
	} f{scene, false};
	obs_source_enum_active_tree(
		program,
		[](obs_source_t *, obs_source_t *child, void *p) {
			auto *f = static_cast<Find *>(p);
			if (child == f->scene)
				f->found = true;
		},
		&f);
	return f.found;
}

} // namespace

OBSSource programOf(obs_canvas_t *canvas)
{
	if (!canvas || isMain(canvas)) {
		OBSSourceAutoRelease out = obs_get_output_source(0);
		OBSSource active = activeOf(out);
		if (active)
			return active;
		OBSSourceAutoRelease current = obs_frontend_get_current_scene();
		return OBSSource(current.Get());
	}
	OBSSourceAutoRelease out = obs_canvas_get_channel(canvas, 0);
	return activeOf(out);
}

bool onProgram(obs_source_t *scene)
{
	if (!scene)
		return false;
	obs_canvas_t *canvas = obs_source_get_canvas(scene);
	OBSSource program = programOf(canvas);
	obs_canvas_release(canvas);
	if (!program)
		return false;
	return program.Get() == scene || (obs_source_active(scene) && nests(program, scene));
}

bool seenByAudience(obs_source_t *scene)
{
	if (!scene)
		return false;
	core::live::Facts f;
	f.onProgram = onProgram(scene);
	if (!f.onProgram)
		return false;
	obs_canvas_t *canvas = obs_source_get_canvas(scene);
	f.mainCanvas = !canvas || isMain(canvas);
	f.mainStreaming = obs_frontend_streaming_active();
	f.mainVirtualCam = obs_frontend_virtualcam_active();
	video_t *video = canvas ? obs_canvas_get_video(canvas) : obs_get_video();
	obs_canvas_release(canvas);
	struct Ctx {
		video_t *video;
		std::vector<core::live::OutputFacts> *out;
	} ctx{video, &f.outputs};
	obs_enum_outputs(
		[](void *p, obs_output_t *output) {
			auto *c = static_cast<Ctx *>(p);
			if (!obs_output_active(output))
				return true;
			core::live::OutputFacts o;
			o.active = true;
			const bool encoded = (obs_output_get_flags(output) & OBS_OUTPUT_ENCODED) != 0;
			o.feedsCanvas = c->video && obs_output_video(output) == c->video;
			if (!o.feedsCanvas && encoded && c->video) {
				/* A scaled encoder has its own video: compare its parent. */
				obs_encoder_t *enc = obs_output_get_video_encoder(output);
				o.feedsCanvas = enc && obs_encoder_parent_video(enc) == c->video;
			}
			o.stream = (obs_output_get_flags(output) & OBS_OUTPUT_SERVICE) != 0;
			const char *id = obs_output_get_id(output);
			o.virtualCamera = id && (std::strcmp(id, kVerticalVirtualCam) == 0 ||
						 std::strcmp(id, "virtualcam_output") == 0);
			c->out->push_back(o);
			return true;
		},
		&ctx);
	return core::live::seenByAudience(f);
}

void tallies(std::set<std::string> &program, std::set<std::string> &preview)
{
	program.clear();
	preview.clear();
	/* Main canvas. */
	OBSSource main = programOf(nullptr);
	if (main) {
		if (obs_obj_is_private(main.Get())) {
			/* Studio Mode with "duplicate scene": the original has the same name. */
			OBSSourceAutoRelease original = obs_get_source_by_name(obs_source_get_name(main));
			if (original)
				program.insert(obs_source_get_uuid(original));
		} else {
			program.insert(obs_source_get_uuid(main));
		}
	}
	if (obs_frontend_preview_program_mode_active()) {
		OBSSourceAutoRelease pvw = obs_frontend_get_current_preview_scene();
		if (pvw)
			preview.insert(obs_source_get_uuid(pvw));
	}
	/* Other canvases. */
	obs_frontend_canvas_list list = {};
	obs_frontend_get_canvases(&list);
	for (size_t i = 0; i < list.canvases.num; i++) {
		OBSSource p = programOf(list.canvases.array[i]);
		if (p)
			program.insert(obs_source_get_uuid(p));
	}
	obs_frontend_canvas_list_free(&list);
}

} // namespace layouts::live
