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

/* Animated layout change inside one scene (AUTO). Runs on program in real time.
 *
 * Threading. Frames are driven by obs_add_tick_callback: the callback runs on the
 * graphics thread, before the sources tick and before the frame is rendered, with the
 * video clock (obs_get_video_frame_time), so the motion is frame-exact and never
 * depends on the Qt event loop. Each frame is written inside obs_scene_atomic_update
 * (the scene's video + audio mutexes, what libobs's own setters rely on), the same way
 * Move Transition writes item transforms from the graphics thread. Nothing else of
 * libobs is touched from the tick: visibility, show/hide transitions, the undo entry
 * and the UI refresh happen on the UI thread (start, and obs_queue_task(OBS_TASK_UI)
 * at the end). The tick callback is registered once (init) and never removed from
 * inside itself.
 *
 * Fades. OBS items have no opacity. Leaving / entering items use their per-item
 * hide / show transition: a private "fade_transition" with the morph duration is set
 * for the time of the morph, the item is hidden / shown, and the item's own transition
 * is put back afterwards. It is per item (a filter would fade the source in every
 * scene that uses it) and is rendered by OBS itself.
 *
 * Merge. Starting a morph on a scene that is morphing takes the items from where they
 * are now (mid-way) and keeps the first "before" state: one undo step in the end.
 *
 * Move Transition (scene to scene) keeps working: the morph only writes the items of
 * its own scene, as a user drag would. */

#include "core/item-model.hpp"

#include <obs.hpp>

#include <vector>

namespace layouts::morph {

struct Target {
	OBSSceneItem item;
	core::ItemTransform t;
	bool visible = true;
};

void init();
void shutdown();

/* The store setting "morph.durationMs" (300 ms by default). */
int defaultDurationMs();

/* Animates the scene items to these states. durationMs < 0 uses the setting, 0 cuts.
 * Items of the scene that are not in targets are left alone. Must be called on the UI
 * thread. Returns false when nothing changes. */
bool applyAnimated(obs_scene_t *scene, const std::vector<Target> &targets, int durationMs = -1);

/* Jumps a running morph of this scene (or of every scene) to its end. UI thread. */
void finish(obs_scene_t *scene = nullptr);

bool running(obs_scene_t *scene);

} // namespace layouts::morph
