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

/* When is the scene being edited really seen by an audience (0.7)? Only then does the
 * Scene Editor warn (red PROGRAM chip, red frame) and, if the option is on, refuse
 * edits. Pure decision; obs/live.cpp gathers the facts. */

#include <vector>

namespace layouts::core::live {

/* An OBS output, as far as the decision goes. */
struct OutputFacts {
	bool active = false;
	bool feedsCanvas = false;   /* its video is the video of the scene's canvas */
	bool stream = false;        /* OBS_OUTPUT_SERVICE: sends to a streaming service */
	bool virtualCamera = false; /* a virtual camera (OBS's, Vertical's) */
};

struct Facts {
	bool onProgram = false;      /* the scene is the program of its canvas (or nested in it) */
	bool mainCanvas = false;     /* the scene belongs to the main canvas */
	bool mainStreaming = false;  /* obs_frontend_streaming_active() */
	bool mainVirtualCam = false; /* obs_frontend_virtualcam_active() */
	std::vector<OutputFacts> outputs;
};

/* True when the scene is on the program of its canvas and that canvas is streamed or
 * sent to a virtual camera. Recording or the replay buffer alone do not count. */
bool seenByAudience(const Facts &facts);

} // namespace layouts::core::live
