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

/* Roles: what a source is for in a show, set by the user per source (0.4).
 *
 *   Host       a camera that takes the large tile in host + guests layouts
 *   Camera     fills its tile, cropped on the face
 *   Screen     shown whole, never cropped
 *   Background full canvas, always at the very back, never in a tile
 *   Graphics   (CG) logo, banner, name, frame: always on top, never moved by layouts
 *   None       not set: ignored by the Builder
 *
 * A role is suggested from the start of the source name ("CAM _ GUEST 1" is a camera),
 * case and accents ignored, before the user confirms or changes it. */

#include <string>
#include <string_view>
#include <vector>

namespace layouts::core {

enum class Role { None, Host, Camera, Screen, Background, Graphics };

/* Host or Camera: placed in a camera tile. */
inline bool isCameraRole(Role r)
{
	return r == Role::Host || r == Role::Camera;
}

/* Stable keys for saving ("none", "host", "camera", "screen", "background", "graphics"). */
const char *roleKey(Role r);
Role roleFromKey(std::string_view key); /* None for unknown keys */

struct PrefixRules {
	std::vector<std::string> host;
	std::vector<std::string> camera;
	std::vector<std::string> screen;
	std::vector<std::string> background;
	std::vector<std::string> graphics;
};

const PrefixRules &defaultPrefixRules();

/* Upper case, accents removed (UTF-8 Latin letters used in French and most western
 * languages), anything else kept as is. */
std::string normalizeName(std::string_view name);

/* Role suggested by the name: a prefix followed by the end of the name, a space, '_',
 * '-' or '.'. The longest matching prefix wins. None when no prefix matches. */
Role roleFromName(std::string_view name, const PrefixRules &rules = defaultPrefixRules());

/* "CAM, CAMERA, CAMÉRA" -> {"CAM", "CAMERA", "CAMÉRA"} (empty items dropped). */
std::vector<std::string> splitPrefixList(std::string_view list);

} // namespace layouts::core
