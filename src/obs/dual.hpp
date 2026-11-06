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

/* Dual output prototype (0.6, behind the "dual.enabled" setting, off by default).
 *
 * Every 16:9 broadcast scene can have a 9:16 twin in a vertical canvas: same sources
 * (shared by reference), arranged by core::mapToPortrait. The vertical canvas is the
 * one another plugin already made (Aitum Vertical...) when there is one, otherwise
 * "Layouts Vertical" 1080x1920. See docs/DUAL-OUTPUT.md. */

#include <obs.hpp>

#include <QString>

#include <string>

namespace layouts::dual {

/* The "dual.enabled" setting. */
bool enabled();

/* The vertical canvas: an existing portrait canvas (name with "vertical" first), or a
 * new "Layouts Vertical" when `create` is set. Strong reference (release it) or null. */
obs_canvas_t *verticalCanvas(bool create);

/* Name of the twin of a scene ("Duo" -> "Duo ▯"). */
QString twinName(const QString &sceneName);

/* Builds or rebuilds the 9:16 twin of a 16:9 scene from its items and their roles
 * (Host/Camera, Screen, Background, Graphics). `key` is the broadcast template the
 * scene was made with ("" when unknown: the twin is generated from the counts).
 * Does nothing unless enabled() or `force`. Returns the twin scene or null. */
OBSSource refreshTwin(obs_source_t *landscapeScene, const std::string &key, bool force = false);

/* CUT both: shows the twin of this scene on the vertical canvas (main channel).
 * False when there is no twin. */
bool cutTwin(obs_source_t *landscapeScene);

} // namespace layouts::dual
