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

#include "roles.hpp"

#include <cctype>
#include <cstring>

namespace layouts::core {

const char *roleKey(Role r)
{
	switch (r) {
	case Role::Camera:
		return "camera";
	case Role::Screen:
		return "screen";
	case Role::Background:
		return "background";
	case Role::Graphics:
		return "graphics";
	case Role::None:
		break;
	}
	return "none";
}

Role roleFromKey(std::string_view key)
{
	for (Role r : {Role::Camera, Role::Screen, Role::Background, Role::Graphics}) {
		if (key == roleKey(r))
			return r;
	}
	return Role::None;
}

const PrefixRules &defaultPrefixRules()
{
	static const PrefixRules rules{
		{"CAM", "CAMERA", "CAMÉRA", "GUEST", "INVITÉ", "HOST"},
		{"SCREEN", "ÉCRAN", "SCR", "DISPLAY", "CAPTURE"},
		{"BG", "BACKGROUND", "FOND"},
		{"CG", "GFX", "OVERLAY", "LOGO", "LT", "HABILLAGE"},
	};
	return rules;
}

namespace {

/* Two-byte UTF-8 sequences (U+00C0..U+00FF) folded to an ASCII upper case letter. */
char foldLatin1(unsigned char second)
{
	/* U+00C0 + i, as the second byte after 0xC3 (0x80 + i). */
	static const char *const map = "AAAAAAACEEEEIIIIDNOOOOO*OUUUUYTSAAAAAAACEEEEIIIIDNOOOOO/OUUUUYTY";
	if (second < 0x80 || second > 0xBF)
		return 0;
	const char c = map[second - 0x80];
	return (c == '*' || c == '/') ? 0 : c;
}

} // namespace

std::string normalizeName(std::string_view name)
{
	std::string out;
	out.reserve(name.size());
	for (size_t i = 0; i < name.size(); i++) {
		const unsigned char c = (unsigned char)name[i];
		if (c == 0xC3 && i + 1 < name.size()) {
			if (const char f = foldLatin1((unsigned char)name[i + 1])) {
				out += f;
				i++;
				continue;
			}
		}
		if (c == 0xC5 && i + 1 < name.size()) {
			const unsigned char d = (unsigned char)name[i + 1];
			if (d == 0x92 || d == 0x93) { /* Œ œ */
				out += "OE";
				i++;
				continue;
			}
		}
		out += (c < 0x80) ? (char)std::toupper(c) : (char)c;
	}
	return out;
}

std::vector<std::string> splitPrefixList(std::string_view list)
{
	std::vector<std::string> out;
	size_t start = 0;
	while (start <= list.size()) {
		size_t end = list.find(',', start);
		if (end == std::string_view::npos)
			end = list.size();
		std::string_view item = list.substr(start, end - start);
		while (!item.empty() && std::isspace((unsigned char)item.front()))
			item.remove_prefix(1);
		while (!item.empty() && std::isspace((unsigned char)item.back()))
			item.remove_suffix(1);
		if (!item.empty())
			out.emplace_back(item);
		start = end + 1;
	}
	return out;
}

Role roleFromName(std::string_view name, const PrefixRules &rules)
{
	const std::string n = normalizeName(name);
	Role best = Role::None;
	size_t bestLength = 0;
	const std::pair<Role, const std::vector<std::string> *> lists[] = {
		{Role::Camera, &rules.camera},
		{Role::Screen, &rules.screen},
		{Role::Background, &rules.background},
		{Role::Graphics, &rules.graphics},
	};
	for (const auto &[role, prefixes] : lists) {
		for (const std::string &raw : *prefixes) {
			const std::string p = normalizeName(raw);
			if (p.empty() || p.size() <= bestLength || n.compare(0, p.size(), p) != 0)
				continue;
			const bool boundary = n.size() == p.size() || std::strchr(" _-.", n[p.size()]) != nullptr;
			if (boundary) {
				best = role;
				bestLength = p.size();
			}
		}
	}
	return best;
}

} // namespace layouts::core
