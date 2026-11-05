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

#pragma once

/* Rounded corners: a filter registered by Layouts ("Rounded corners") that masks the
 * source with anti-aliased rounded corners. The mask follows the crop of the scene item
 * being drawn, so a cropped camera keeps its corners.
 *
 * The radius belongs to the scene: the filter keeps one value per scene (in source
 * pixels), and a small filter on each such scene ("Rounded corners: scene") tells it which
 * scene is rendering. The same camera can be round in one scene and square in another.
 * The Inspector shows and edits the radius in canvas pixels. */

#include "core/expression.hpp"

#include <obs.hpp>

#include <string>

namespace layouts {

struct ItemEntry;

namespace corners {

constexpr const char *kFilterId = "layouts_rounded_corners";
constexpr const char *kSceneFilterId = "layouts_scene_corners";

void registerFilter();

/* Remembers the transition on the program (called on OBS events; self-test after setting
 * the output channel itself). */
void watchProgram();

/* Radius of an item in canvas pixels, 0 when its corners are square. */
double radius(const ItemEntry &e);

/* Sets the radius (canvas pixels) of the selected items in the edited scene only, as one
 * undo step. The filter goes away when no scene has corners left. Groups are skipped. */
bool setRadius(const core::Expression &expr);

/* Field label drag: applied live, one undo step once finished. */
void scrub(double delta, bool finished);

/* ---- shared with the border and shadow filter (style-render.cpp) ---- */

/* Token of the scene rendering on this thread (scene marker), empty outside a scene. */
std::string renderingScene();
/* Token of a scene, created when missing (UI thread). */
std::string sceneToken(obs_source_t *scene);
/* Token of a scene read from its marker, render thread safe; empty when it has none. */
std::string sceneTokenAtRender(obs_source_t *scene);
/* The transition on the program, as last seen from the UI thread (render thread safe). */
OBSSource programTransition();
/* Corner radius of a source for the render in progress (source pixels, horizontal, and
 * vertical/horizontal ratio); false when the source has no corners filter. */
bool renderRadius(obs_source_t *source, float &r, float &ratio);

} // namespace corners
} // namespace layouts
