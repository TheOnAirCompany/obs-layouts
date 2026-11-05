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

/* The gallery of broadcast layouts ("Layouts plateau"): thumbnails of the templates that
 * fit a number of cameras and screens, drawn in the canvas shape. Shared by the Inspector
 * (Scene mode) and the Builder. The template key is in Qt::UserRole of each item.
 * Layouts saved by the user (0.5) carry a small corner marker and a context menu:
 * Rename, Delete, Export..., Import layouts... */

#include <QListWidget>
#include <QStringList>

#include <string>

namespace layouts::ui {

class BroadcastGallery : public QListWidget {
	Q_OBJECT

public:
	explicit BroadcastGallery(QWidget *parent = nullptr);

	/* Shows these templates; keeps the current one when still there, else the first. */
	void setTemplates(const QStringList &keys, double aspect);
	QString currentKey() const;
	QString currentLabel() const;
	void fitRows();

	/* Call after any change to the user layouts: every gallery redraws on its next sync. */
	static void userLayoutsChanged();

protected:
	void resizeEvent(QResizeEvent *e) override;
	void contextMenuEvent(QContextMenuEvent *e) override;
};

/* "Save as layout...": asks a name, saves the scene being edited (selection, or every
 * visible camera, screen and chat). Returns false when cancelled or nothing to save. */
bool saveLayoutDialog(QWidget *parent);
/* "Import layouts...": picks .json files and merges them. */
void importLayoutsDialog(QWidget *parent);

} // namespace layouts::ui
