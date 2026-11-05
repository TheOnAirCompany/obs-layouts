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

/* User layouts on the OBS side (0.5): saved from the scene being edited, kept in the
 * plugin config directory (user-layouts.json, format of core/user-layouts.hpp) so that
 * they are shared by every scene collection, exported and imported as files. */

#include <QString>

#include <string>
#include <vector>

namespace layouts::userlayouts {

/* Reads user-layouts.json into the registry (call once at load). */
void load();
/* Writes the registry to user-layouts.json. */
bool save();

/* Number of tiles a layout made now would have (selection, else every visible camera,
 * screen and chat of the scene). */
int capturableCount();

/* Makes a layout from the scene being edited: the selected items, or every visible
 * top-level camera / screen / chat when nothing is selected. Each item gives a slot at
 * its visible place (crop, bounds and rotation included), its kind from its role.
 * Saved at once. Returns the new key, empty when there was nothing to save. */
std::string saveFromScene(const QString &name);

bool rename(const std::string &key, const QString &name);
bool remove(const std::string &key);

/* Writes these layouts (all when empty) to a .json file. */
bool exportTo(const QString &path, const std::vector<std::string> &keys, QString *error = nullptr);
/* Adds the layouts of a .json file: same key and content skipped, conflicts renamed.
 * Returns the number added, -1 when the file cannot be read. */
int importFrom(const QString &path, QString *error = nullptr);

} // namespace layouts::userlayouts
