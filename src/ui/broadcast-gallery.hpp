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

#include <QColor>
#include <QIcon>
#include <QListWidget>
#include <QStringList>

class QToolButton;

#include <string>

namespace layouts::ui {

/* Thumbnail of a template in the canvas shape (also used by the Switcher dock). */
QIcon broadcastThumb(const std::string &key, const QColor &camera, const QColor &screen, const QColor &bg,
		     double aspect);

class BroadcastGallery : public QListWidget {
	Q_OBJECT

public:
	explicit BroadcastGallery(QWidget *parent = nullptr);

	/* Shows these templates; keeps the current one when still there, else the first. */
	void setTemplates(const QStringList &keys, double aspect);
	QString currentKey() const;
	QString currentLabel() const;
	void fitRows();

	/* Mirrored layouts (persisted in the store): thumbnails and every apply / new
	 * scene use the mirrored slots. */
	bool mirrorH() const;
	bool mirrorV() const;
	/* The two mirror toggles, to place next to the gallery (created on first call,
	 * shown and hidden with the gallery). */
	QWidget *mirrorBar();

	void setVisible(bool visible) override;

	/* Call after any change to the user layouts: every gallery redraws on its next sync. */
	static void userLayoutsChanged();

signals:
	/* The user toggled a mirror button. */
	void mirrorChanged();

protected:
	void resizeEvent(QResizeEvent *e) override;
	void contextMenuEvent(QContextMenuEvent *e) override;

private:
	void renderThumbs();
	void syncMirrorButtons();

	QWidget *mirrorBar_ = nullptr;
	QToolButton *mirrorH_ = nullptr;
	QToolButton *mirrorV_ = nullptr;
	int renderedMirror_ = 0; /* bit 0 = h, bit 1 = v, of the drawn thumbnails */
};

/* "Save as layout...": asks a name, saves the scene being edited (selection, or every
 * visible camera, screen and chat). Returns false when cancelled or nothing to save. */
bool saveLayoutDialog(QWidget *parent);
/* "Import layouts...": picks .json files and merges them. */
void importLayoutsDialog(QWidget *parent);

} // namespace layouts::ui
