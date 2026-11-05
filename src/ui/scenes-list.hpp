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

/* Scenes above Layers (like the pages of a design tool): every scene, the first row
 * follows OBS. Clicking a scene edits it in the Layouts window without changing what is
 * on air; the scene on the program has a red dot. */

#include <obs-frontend-api.h>

#include <QListWidget>

namespace layouts::ui {

class ScenesList : public QListWidget {
	Q_OBJECT

public:
	explicit ScenesList(QWidget *parent = nullptr);
	~ScenesList() override;

	void refresh();

private:
	static void onFrontendEvent(enum obs_frontend_event event, void *data);
	void onClicked(QListWidgetItem *item);

	bool updating_ = false;
};

} // namespace layouts::ui
