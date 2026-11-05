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

/* Layouts Builder: makes the scenes of a show from typed sources, whatever scene they
 * are in. Each scene gets the background at the very back, the hosts, cameras and
 * screens in the tiles of a broadcast layout, and the graphics on top at the place
 * they already have elsewhere. */

#include "core/broadcast.hpp"
#include "core/roles.hpp"

#include <obs.hpp>

#include <QString>

#include <functional>
#include <string>
#include <vector>

namespace layouts::builder {

struct Options {
	core::BroadcastSpec spec;
	bool solo = true;       /* a full-screen scene per host, camera and screen */
	bool layouts = true;    /* a scene per broadcast layout that fits */
	bool background = true; /* the background in every scene */
	bool graphics = true;   /* the graphics in every scene */
	bool lock = true;
};

struct SceneSpec {
	std::string key; /* broadcast template */
	QString name;    /* wanted name (made unique when created) */
	std::vector<OBSSource> cameras;
	std::vector<OBSSource> screens;
};

struct Plan {
	std::vector<OBSSource> cameras; /* hosts first */
	std::vector<OBSSource> screens;
	OBSSource background; /* the first one */
	std::vector<OBSSource> graphics;
	std::vector<SceneSpec> scenes;
};

using TemplateName = std::function<QString(const std::string &key)>;
using SoloName = std::function<QString(const QString &sourceName)>;

/* What would be created for these sources (their roles decide; None is ignored). */
Plan plan(const std::vector<OBSSource> &sources, const Options &options, const TemplateName &templateName,
	  const SoloName &soloName);

/* One scene with this broadcast template for these sources (background and graphics
 * among them go at the back and on top). */
Plan single(const std::vector<OBSSource> &sources, const std::string &key, const QString &name);

/* Creates the planned scenes in the canvas of the edited scene. Returns the names. */
std::vector<QString> create(const Plan &plan, const Options &options);

} // namespace layouts::builder
