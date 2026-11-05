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

#include <QColor>
#include <QIcon>
#include <QListWidget>
#include <QStringList>

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

protected:
	void resizeEvent(QResizeEvent *e) override;
};

} // namespace layouts::ui
