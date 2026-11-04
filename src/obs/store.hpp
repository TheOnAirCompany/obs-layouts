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

/* Persistence.
 *  - Guides belong to the scene collection: saved through obs_frontend_add_save_callback
 *    under the "layouts" key, so they follow the collection (export, backups).
 *  - View settings (rulers, grid, snapping, safe areas, live policy) are global
 *    preferences, saved as JSON in the plugin config directory.
 * The native collection file is never edited directly. */

#include "core/grid.hpp"
#include "core/guides.hpp"
#include "core/snapping.hpp"

#include <obs.h>

#include <QColor>
#include <QObject>

namespace layouts {

enum class GridKind { Square, Columns, Rows };

struct ViewSettings {
	bool rulers = true;
	bool guides = true;
	bool grid = false;
	bool safeAreas = false;
	bool centerLines = false;
	bool outlines = true; /* thin outline on every item */
	GridKind gridKind = GridKind::Square;
	core::SquareGrid square{10.0, 0.0, 0.0};
	core::TrackGrid columns{12, 20.0, 40.0, 80.0, core::TrackAlign::Stretch};
	core::TrackGrid rows{6, 20.0, 40.0, 80.0, core::TrackAlign::Stretch};
	core::SnapSettings snap;
	double customMarginX = 0.0; /* custom safe margins in canvas px, 0 = off */
	double customMarginY = 0.0;
	int refPoint = 0;        /* 0..8, reference point for X/Y/W/H */
	bool blockOnAir = false; /* refuse edits on a scene that is on air */
	bool ratioLock = false;
	bool firstRunDone = false; /* docks placed on first launch */
	QColor guideColor{0xFF, 0x3D, 0x71};
};

class Store : public QObject {
	Q_OBJECT

public:
	static Store &instance();

	core::GuideDocument &guides() { return guides_; }
	ViewSettings &settings() { return settings_; }

	void loadSettings();
	void saveSettings();

	/* obs_frontend save callback */
	void saveGuides(obs_data_t *data);
	void loadGuides(obs_data_t *data);

	/* Call after any change to guides / settings so every view refreshes. */
	void notifyGuidesChanged();
	void notifySettingsChanged(bool persist = true);

signals:
	void guidesChanged();
	void settingsChanged();

private:
	Store() = default;
	core::GuideDocument guides_;
	ViewSettings settings_;
};

} // namespace layouts
