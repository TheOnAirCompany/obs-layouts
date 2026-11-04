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

/* Global OBS hotkeys for Layouts actions (Settings > Hotkeys). No key is bound by
 * default. Bindings are saved in the plugin config directory (hotkeys.json). */

#include <functional>

namespace layouts::hotkeys {

/* Registers every action; the callbacks serve "zoom to fit" and "open Layouts". */
void registerAll(std::function<void()> zoomFit, std::function<void()> openWindow);
void save();
void unregisterAll();

/* Runs the action registered under this name (self-test). */
bool trigger(const char *name);

} // namespace layouts::hotkeys
