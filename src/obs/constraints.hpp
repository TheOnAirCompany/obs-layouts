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

/* Constraints: how a scene item follows a change of its canvas size.
 *
 * Stored in the item's private settings (saved with the scene collection). When a
 * canvas is reset to another size (OBS Settings > Video, or another canvas), the
 * global "canvas_video_reset" signal triggers a pass over every scene of that canvas:
 * the rect each constrained item had before is recovered from the size change, and
 * the constraint is applied. Items without constraints keep OBS's own behavior. */

#include "core/layout.hpp"

#include <obs.h>

#include <QObject>
#include <QString>

#include <map>
#include <utility>

namespace layouts {

void getConstraints(obs_sceneitem_t *item, core::HConstraint &h, core::VConstraint &v);
void setConstraints(obs_sceneitem_t *item, core::HConstraint h, core::VConstraint v);

class ConstraintWatcher : public QObject {
	Q_OBJECT

public:
	static ConstraintWatcher &instance();

	void start();
	void stop();
	/* Remembers the current size of every canvas (after loading, canvas added). */
	void recordSizes();

public slots:
	void onCanvasReset(const QString &uuid);

private:
	ConstraintWatcher() = default;
	std::map<std::string, std::pair<uint32_t, uint32_t>> sizes_;
	bool started_ = false;
};

} // namespace layouts
