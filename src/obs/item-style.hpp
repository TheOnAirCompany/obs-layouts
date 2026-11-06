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

/* Item style (0.5) on scene items: the private setting "layouts_style" holds an item
 * override (same JSON as core::toJson); without it the item follows the default of
 * its source type (Store::typeStyle). Every edit made here is one OBS undo step. */

#include "core/roles.hpp"
#include "core/style.hpp"

#include <obs.h>

#include <functional>
#include <vector>

namespace layouts {

namespace itemstyle {

constexpr const char *kSettingKey = "layouts_style";

core::Role itemRole(obs_sceneitem_t *item);
/* The item carries its own style. */
bool hasOverride(obs_sceneitem_t *item);
/* Default of the item's source type. */
core::Style typeDefault(obs_sceneitem_t *item);
/* Override if any, otherwise the type default. */
core::Style effective(obs_sceneitem_t *item);
/* An override that is not the type default ("override" dot in the Inspector). */
bool differsFromType(obs_sceneitem_t *item);

/* Same style (every field). */
bool sameStyle(const core::Style &a, const core::Style &b);

/* Changes the style of these items (each one keeps its other values), one undo step. */
void edit(const std::vector<obs_sceneitem_t *> &items, const std::function<void(core::Style &)> &change);
/* Removes the overrides: the items follow their type again, one undo step. */
void resetToType(const std::vector<obs_sceneitem_t *> &items);

/* Draws the item with its effective style (corners, border, shadow). */
void render(obs_sceneitem_t *item);
/* After a type default changed: every item of this type in the current collection
 * that has no override. */
void reapplyType(core::Role role);

} // namespace itemstyle
} // namespace layouts
