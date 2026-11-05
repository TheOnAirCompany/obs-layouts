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

/* Role of each source (Host, Camera, Screen, Background, Graphics), saved on the source in the
 * scene collection so it survives renaming. Until the user sets one, a role is
 * suggested from the name prefix, then from the kind of source (video capture device =
 * camera, display or window capture = screen). */

#include "core/roles.hpp"

#include <obs.hpp>

#include <vector>

namespace layouts::roles {

/* The role the user set, None if never set. */
core::Role explicitRole(obs_source_t *source);
/* What Layouts proposes when nothing is set. */
core::Role suggestedRole(obs_source_t *source);
/* The explicit role if set, otherwise the suggestion. */
core::Role role(obs_source_t *source);

/* Sets the role of these sources as one undo step ("Set role"). */
void setRole(const std::vector<obs_source_t *> &sources, core::Role role);

/* Sources that show video and can be placed: inputs only (no scenes, groups,
 * transitions or filters), in creation order. */
std::vector<OBSSource> videoSources();
/* Number of scenes (any canvas) that use this source, groups included. */
int sceneCount(obs_source_t *source);

/* Kind of source, from its type id. */
bool isCameraKind(obs_source_t *source);
bool isScreenKind(obs_source_t *source);

} // namespace layouts::roles
