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

/* What is on the program of each canvas, what is in preview, and whether a scene is
 * really seen by an audience (core/live.hpp decides). UI thread. */

#include <obs.hpp>

#include <set>
#include <string>

namespace layouts::live {

/* The scene shown on the program of a canvas (the main one: what the main output
 * really renders, a private copy in Studio Mode with "duplicate scene"). New ref. */
OBSSource programOf(obs_canvas_t *canvas);

/* True when the scene is the program of its canvas or nested in it. */
bool onProgram(obs_source_t *scene);

/* On the program of its canvas, and that canvas is streamed or sent to a virtual camera. */
bool seenByAudience(obs_source_t *scene);

/* Uuids of the scenes on program (every canvas) and in preview (Studio Mode). A private
 * copy on the main program counts as its original (same name). */
void tallies(std::set<std::string> &program, std::set<std::string> &preview);

} // namespace layouts::live
