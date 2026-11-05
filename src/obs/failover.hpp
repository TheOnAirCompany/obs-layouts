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

/* Live failover. When a camera or a screen of the live layout drops, the layout goes to
 * one tile less directly on program (an emergency: no transition, no preview step), a red
 * alert is raised, and the original layout comes back by itself once the source is back.
 *
 * The Régie tells which layout is live with arm(): the scene, its template key and the
 * source of each slot. Works with and without Studio Mode (the scene on program is
 * obs_frontend_get_current_scene in both). Off when "failover.enabled" is off. */

#include "core/failover.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

#include <QObject>
#include <QString>

#include <set>
#include <string>
#include <vector>

namespace layouts {

class Failover : public QObject {
	Q_OBJECT

public:
	static Failover &instance();

	/* The layout `key` filled with `assignment` (source name per slot, slot order) is on
	 * `scene`. Call again whenever the Régie applies another layout; a failover running on
	 * the previous one is ended first (its layout restored). */
	void arm(const QString &scene, const std::string &key, const core::SlotAssignment &assignment);
	void disarm();

	/* Frontend events (scene changed on program, collection changing, exit). */
	void handleFrontendEvent(enum obs_frontend_event event);

	bool active() const { return applied_; }
	const std::set<std::string> &lostSources() const { return lost_; }
	/* Layout shown now: the fallback while active, else the armed one. */
	std::string currentKey() const { return applied_ ? fallbackKey_ : key_; }

signals:
	/* Red banner of the Régie: active = a failover is running, message says what dropped.
	 * active = false: everything is back, hide the banner. */
	void alert(const QString &message, bool active);

private:
	Failover();
	void onLost(const QString &name);
	void onBack(const QString &name);
	void update();
	bool sceneOnProgram() const;
	void restore();

	struct Snapshot {
		std::string name;
		obs_transform_info info;
		obs_sceneitem_crop crop;
		bool visible;
		bool held = false; /* obs_source_inc_showing called while hidden */
	};

	QString scene_;
	std::string key_;
	core::SlotAssignment assignment_;
	std::set<std::string> lost_;
	std::vector<Snapshot> snapshot_; /* original layout, taken at the first drop */
	bool applied_ = false;
	std::string fallbackKey_;
};

} // namespace layouts
