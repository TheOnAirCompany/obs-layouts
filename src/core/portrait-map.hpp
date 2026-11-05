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

/* Dual output (0.6): the 9:16 twin of a 16:9 broadcast layout.
 *
 * A 16:9 scene is a template key plus slots filled with sources. Its vertical twin
 * shows the same sources on a portrait canvas. For the templates that know how to draw
 * themselves in portrait (the 23 legacy ones), the twin is the same template built on
 * the portrait canvas. For the others (landscape-only templates, custom layouts), the
 * twin is generated from the slot counts: screens on top across the full width,
 * cameras stacked or in a grid below, chat at the bottom.
 *
 * The result keeps a link from each 16:9 slot to its 9:16 slot so the OBS layer puts the
 * same source (by reference) in both. See docs/DUAL-OUTPUT.md. */

#include "broadcast.hpp"

#include <string>
#include <vector>

namespace layouts::core {

enum class PortraitStrategy {
	Legacy,    /* buildBroadcast(key) on the portrait canvas */
	Generated, /* arranged from the counts */
};

struct PortraitMap {
	PortraitStrategy strategy = PortraitStrategy::Generated;
	/* Layout key of the twin: the 16:9 key for Legacy, "Portrait/<c>c<s>s<h>h" otherwise.
	 * Stable for the same counts so the twin can be recognized and refreshed. */
	std::string key;
	std::vector<Slot> slots; /* on the portrait canvas: cameras, then screens, then chats */
	/* For each 16:9 slot (same index as the input), the index of its twin slot in
	 * `slots`, or -1 when it has none. Kinds always match. */
	std::vector<int> twinOf;
};

/* True when `key` is a known template that draws itself in portrait. */
bool supportsPortrait(const std::string &key);

/* The best 9:16 arrangement for a 16:9 layout. `landscape` holds the slots of the 16:9
 * scene in their order (as given by buildBroadcast, or read back from a custom layout);
 * only kind, host, ticker and shape are used. `portrait` is the vertical canvas. */
PortraitMap mapToPortrait(const std::string &key, const std::vector<Slot> &landscape, const Rect &portrait,
			  const BroadcastSpec &spec);

/* Shortcut for a known template: builds its 16:9 slots on `landscape` first. */
PortraitMap mapTemplateToPortrait(const std::string &key, const Rect &landscape, const Rect &portrait,
				  const BroadcastSpec &spec);

} // namespace layouts::core
