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
 * (Scene mode) and the Builder. The template key is in Qt::UserRole of each item. */

#include <QListWidget>
#include <QStringList>

class QToolButton;

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

	/* Mirrored layouts (persisted in the store): thumbnails and every apply / new
	 * scene use the mirrored slots. */
	bool mirrorH() const;
	bool mirrorV() const;
	/* The two mirror toggles, to place next to the gallery (created on first call,
	 * shown and hidden with the gallery). */
	QWidget *mirrorBar();

	void setVisible(bool visible) override;

signals:
	/* The user toggled a mirror button. */
	void mirrorChanged();

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	void renderThumbs();
	void syncMirrorButtons();

	QWidget *mirrorBar_ = nullptr;
	QToolButton *mirrorH_ = nullptr;
	QToolButton *mirrorV_ = nullptr;
	int renderedMirror_ = 0; /* bit 0 = h, bit 1 = v, of the drawn thumbnails */
};

} // namespace layouts::ui
