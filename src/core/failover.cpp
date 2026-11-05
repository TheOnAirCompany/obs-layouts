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

#include "failover.hpp"

#include "broadcast.hpp"

#include <cctype>

namespace layouts::core {

std::string templateFamily(const std::string &key)
{
	std::string out;
	bool inNumber = false;
	for (char c : key) {
		if (std::isdigit((unsigned char)c)) {
			if (!inNumber)
				out += '#';
			inNumber = true;
		} else {
			out += c;
			inNumber = false;
		}
	}
	return out;
}

FailoverChoice chooseFallback(const std::string &key, const SlotAssignment &assignment,
			      const std::set<std::string> &lost, bool portrait)
{
	FailoverChoice r;
	const BroadcastTemplate *current = nullptr;
	for (const BroadcastTemplate &t : broadcastTemplates()) {
		if (t.key == key)
			current = &t;
	}
	if (!current)
		return r;

	/* Split the assignment by slot kind, without the lost sources. */
	std::vector<std::string> groups[3];
	const int counts[3] = {current->cameras, current->screens, current->chats};
	size_t index = 0;
	bool anyLost = false;
	for (int g = 0; g < 3; g++) {
		for (int i = 0; i < counts[g]; i++, index++) {
			const std::string name = index < assignment.size() ? assignment[index] : std::string();
			if (!name.empty() && lost.count(name)) {
				anyLost = true;
				continue;
			}
			groups[g].push_back(name);
		}
	}
	if (!anyLost) {
		r.ok = true;
		r.key = key;
		r.assignment = assignment;
		return r;
	}

	const int cams = (int)groups[0].size();
	const int screens = (int)groups[1].size();
	if (cams + screens == 0)
		return r;

	const std::string family = templateFamily(key);
	const BroadcastTemplate *best = nullptr;
	int bestScore = -1;
	for (const BroadcastTemplate &t : broadcastTemplates()) {
		if (t.cameras != cams || t.screens != screens || (portrait && t.landscapeOnly))
			continue;
		const int score = (templateFamily(t.key) == family ? 2 : 0) + (t.chats == current->chats ? 1 : 0);
		if (score > bestScore) { /* strict: on a tie the first one in display order wins */
			best = &t;
			bestScore = score;
		}
	}
	if (!best)
		return r;

	r.ok = true;
	r.key = best->key;
	for (const std::string &n : groups[0])
		r.assignment.push_back(n);
	for (const std::string &n : groups[1])
		r.assignment.push_back(n);
	for (int i = 0; i < best->chats; i++)
		r.assignment.push_back(i < (int)groups[2].size() ? groups[2][i] : std::string());
	return r;
}

} // namespace layouts::core
