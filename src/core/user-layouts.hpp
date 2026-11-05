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

/* User layouts (0.5): broadcast layouts saved from a scene arranged by hand. They sit in
 * the gallery next to the built-in templates under keys "user:<id>".
 *
 * Slot cells are stored normalized to the canvas they were drawn on (0..1 on each
 * axis), so a layout drawn on 1920x1080 also applies to 1280x720. On a canvas of
 * another shape the cells are stretched with the canvas (rotation and shapes kept).
 *
 * JSON format "layouts-user-layout/1", written and read without any library:
 *   {"format": "layouts-user-layout/1",
 *    "layouts": [{"key": "user:...", "name": "...", "aspect": 1.7778,
 *                 "slots": [{"kind": "camera", "x": 0, "y": 0, "w": 0.5, "h": 1,
 *                            "rotation": 0, "shape": "rect", "polygon": [[x, y], ...],
 *                            "frame": false, "ticker": false, "host": false}]}]}
 * Unknown fields are ignored, missing ones take their default. */

#include "broadcast.hpp"

#include <string>
#include <vector>

namespace layouts::core {

inline constexpr const char *kUserLayoutFormat = "layouts-user-layout/1";
inline constexpr const char *kUserKeyPrefix = "user:";

struct UserLayout {
	std::string key;  /* "user:<id>" */
	std::string name; /* shown in the gallery */
	double aspect = 16.0 / 9.0;
	std::vector<Slot> tiles; /* cells normalized to 0..1 of the canvas */

	int cameras() const;
	int screens() const;
	int chats() const;
};

inline bool isUserKey(const std::string &key)
{
	return key.rfind(kUserKeyPrefix, 0) == 0;
}

/* Canvas-space slots -> normalized slots, in the order buildBroadcast promises: hosts
 * first, then cameras in reading order, then screens, then chats. */
std::vector<Slot> normalizeSlots(const std::vector<Slot> &world, const Rect &canvas);

/* The slots of a user layout on a canvas. */
std::vector<Slot> buildUserLayout(const UserLayout &layout, const Rect &canvas);

std::string userLayoutsToJson(const std::vector<UserLayout> &layouts);
/* False when the text is not JSON or not in a known format. Slots with an unknown kind or
 * an empty cell are dropped, layouts without slots too. */
bool userLayoutsFromJson(const std::string &json, std::vector<UserLayout> &out, std::string *error = nullptr);

/* The layouts known to buildBroadcast / templatesFor. Not thread-safe: UI thread only. */
class UserLayoutRegistry {
public:
	static UserLayoutRegistry &instance();

	const std::vector<UserLayout> &all() const { return layouts_; }
	void setAll(std::vector<UserLayout> layouts) { layouts_ = std::move(layouts); }
	const UserLayout *find(const std::string &key) const;

	/* Adds a layout under a new unique key (the given key is ignored). Returns the key. */
	std::string add(UserLayout layout);
	bool rename(const std::string &key, const std::string &name);
	bool remove(const std::string &key);

	/* Import: a key already here with the same content is skipped; a key already here
	 * with another content gets a new key, and its name a suffix when the name is taken
	 * too ("Duo (2)"). Returns the number of layouts added. */
	int merge(const std::vector<UserLayout> &incoming);

	/* A name not used by any layout: `base`, else "base (2)", "base (3)"... */
	std::string uniqueName(const std::string &base) const;

private:
	std::string newKey() const;
	std::vector<UserLayout> layouts_;
};

} // namespace layouts::core
